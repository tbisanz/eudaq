#include "ui_euRun.h"
#include "ui_euRun.h"
#include "RunControlModel.hh"
#include "eudaq/RunControl.hh"
#include "eudaq/Utils.hh"
#include <QFileDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QCloseEvent>
#include <QItemDelegate>
#include <QDir>
#include <QPainter>
#include <QTimer>
#include <QInputDialog>
#include <QSettings>

using eudaq::to_string;
using eudaq::from_string;

inline std::string to_bytes(const std::string &val) {
  if (val == "")
    return "";
  uint64_t n = from_string(val, 0ULL);
  const char *suff[] = {" B", " kB", " MB", " GB", " TB"};
  const int numsuff = sizeof suff / sizeof *suff;
  int mult = 0;
  while (n / 1024 >= 10 && mult < numsuff - 1) {
    n /= 1024;
    mult++;
  }
  return to_string(n) + suff[mult];
}

// To make Qt behave on OSX (to be checked on other OSes)
#define MAGIC_NUMBER 22

class RunConnectionDelegate : public QItemDelegate {
public:
  RunConnectionDelegate(RunControlModel *model);

private:
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const;
  RunControlModel *m_model;
};

class RunControlGUI : public QMainWindow,
                      public Ui::wndRun,
                      public eudaq::RunControl {
  Q_OBJECT
public:
  RunControlGUI(const std::string &listenaddress, QRect geom,
                QWidget *parent = 0, Qt::WindowFlags flags = 0);
  ~RunControlGUI();

private:
  enum state_t { ST_NONE, ST_CONFIGLOADED, ST_READY, ST_RUNNING };
  QString lastUsedDirectory = "";
  QStringList allConfigFiles;
  virtual void OnConnect(const eudaq::ConnectionInfo &id);
  virtual void OnDisconnect(const eudaq::ConnectionInfo &id) {
    m_run.disconnected(id);
  }
  virtual void OnReceive(const eudaq::ConnectionInfo &id,
                         std::shared_ptr<eudaq::Status> status);
  void EmitStatus(const char *name, const std::string &val) {
    if (val == "")
      return;
    emit StatusChanged(name, val.c_str());
  }

  void closeEvent(QCloseEvent *event) {
    if (m_run.rowCount() > 0 &&
        QMessageBox::question(
            this, "Quitting", "Terminate all connections and quit?",
            QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
      event->ignore();
    } else {
      Terminate();
      event->accept();
    }
  }
  bool eventFilter(QObject *object, QEvent *event);
private slots:

  void SetStateSlot(int state) {
    btnLoad->setEnabled(state != ST_RUNNING);
    btnConfig->setEnabled((state == ST_CONFIGLOADED) || (state == ST_READY));
    btnTerminate->setEnabled(state != ST_RUNNING);
    btnStart->setEnabled(state == ST_READY);
    btnStop->setEnabled(state == ST_RUNNING);
  }

  void on_btnTerminate_clicked() { close(); }

  void on_btnConfig_clicked() {
    QDir dir(lastUsedDirectory, "*.conf");
    // allConfigFiles
    for (size_t i = 0; i < dir.count(); ++i) {
      QString item = dir[i];
      item.chop(5);
      allConfigFiles.append(item);
    }
    std::string settings = txtConfigFileName->text().toStdString();
    Configure(settings, txtGeoID->text().toInt());
    SetState(ST_READY);
    dostatus = true;
  }
  // void on_btnReset_clicked() {
  //  Reset();
  //}
  void on_btnStart_clicked(bool cont = false) {
    m_prevtrigs = 0;
    m_prevtime = 0.0;
    m_runstarttime = 0.0;
    StartRun(cont ? "Continued" : txtRunmsg->displayText().toStdString());
    EmitStatus("RUN", to_string(m_runnumber));
    emit StatusChanged("EVENT", "0");
    emit StatusChanged("TRIG", "0");
    emit StatusChanged("PARTICLES", "0");
    emit StatusChanged("RATE", "");
    emit StatusChanged("MEANRATE", "");
    SetState(ST_RUNNING);
  }
  void on_btnStop_clicked() {
    StopRun();
    EmitStatus("RUN", "(" + to_string(m_runnumber) + ")");
    SetState(ST_READY);
  }
  void on_btnLog_clicked() {
    std::string msg = txtLogmsg->displayText().toStdString();
    EUDAQ_USER(msg);
  }
  void on_btnLoad_clicked() {
    // QString tempLastFileName;
    // tempLastFileName = txtConfigFileName->text();

    QString temporaryFileName = QFileDialog::getOpenFileName(
        this, tr("Open File"), lastUsedDirectory, tr("*.conf (*.conf)"));

    if (!temporaryFileName.isNull()) {
      txtConfigFileName->setText(temporaryFileName);
      lastUsedDirectory =
          QFileInfo(temporaryFileName).path(); // store path for next time
      SetState(ST_CONFIGLOADED);
    }
  }
  void timer() {
    if (!m_stopping) {
      if ((m_runsizelimit >= 1024 && m_filebytes >= m_runsizelimit) ||
          (m_runeventlimit >= 1 && m_events >= m_runeventlimit)) {
        if (m_runsizelimit >= 1024 && m_filebytes >= m_runsizelimit) {
          EUDAQ_INFO("File limit reached: " + to_string(m_filebytes) + " > " +
                     to_string(m_runsizelimit));
        } else {
          EUDAQ_INFO("Event limit reached: " + to_string(m_events) + " > " +
                     to_string(m_runeventlimit));
        }
        eudaq::mSleep(1000);
        StopRun(false);
        eudaq::mSleep(20000);
        if (m_nextconfigonrunchange) {
          if (allConfigFiles.indexOf(
                  QRegExp("^" + QRegExp::escape(txtConfigFileName->text()))) +
                  1 <
              allConfigFiles.count()) {
            EUDAQ_INFO("Moving to next config file and starting a new run");
            txtConfigFileName->setText(allConfigFiles.at(allConfigFiles.indexOf(
                QRegExp("^" + QRegExp::escape(txtConfigFileName->text())))));
            on_btnConfig_clicked();
            eudaq::mSleep(1000);
            m_startrunwhenready = true;
          } else
            EUDAQ_INFO("All config files processed.");
        } else
          on_btnStart_clicked(true);
      } else if (dostatus) {
        GetStatus();
      }
    }
    if (m_startrunwhenready && !m_producer_pALPIDEfs_not_ok &&
        !m_producer_pALPIDEss_not_ok) {
      m_startrunwhenready = false;
      on_btnStart_clicked(false);
    }
  }
  void ChangeStatus(const QString &name, const QString &value) {
    status_t::iterator i = m_status.find(name.toStdString());
    if (i != m_status.end()) {
      i->second->setText(value);
    }
  }
  void btnLogSetStatusSlot(bool status) { btnLog->setEnabled(status); }

signals:
  void StatusChanged(const QString &, const QString &);
  void btnLogSetStatus(bool status);
  void SetState(int status);

private:
  RunControlModel m_run;
  RunConnectionDelegate m_delegate;
  QTimer m_statustimer;
  typedef std::map<std::string, QLabel *> status_t;
  status_t m_status;
  int m_prevtrigs;
  double m_prevtime, m_runstarttime;
  int64_t m_filebytes;
  int64_t m_events;
  bool dostatus;
  // Using automatic configuration file changes, the two variables below are
  // needed to ensure that the producers finished their configuration, before
  // the next run is started. If that is covered by the control FSM in a later
  // version of the software, these variables and the respective code need to
  // be removed again.
  bool m_producer_pALPIDEfs_not_ok;
  bool m_producer_pALPIDEss_not_ok;
  bool m_startrunwhenready;
};
