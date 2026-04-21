#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

class ProgressDialog : public QDialog
{
    Q_OBJECT

private:
    QProgressBar* m_progressBar;
    QLabel* m_titleLabel;
    QLabel* m_fileLabel;
    QLabel* m_percentLabel;
    QLabel* m_detailLabel;
    QPushButton* m_cancelBtn;

    bool m_isFinished;

public:
    explicit ProgressDialog(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setFileName(const QString& file_name);
    void setProgress(qint64 current, qint64 total);
    void setFinished();

signals:
    void cancelled();  // 用户取消

private slots:
    void onCancelClicked();

};

#endif // PROGRESSDIALOG_H
