#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMap>
#include <QFrame>

enum class TransferDirection { Upload, Download };

// ========== 单个传输项 ==========
class TransferItem : public QFrame
{
    Q_OBJECT

public:
    explicit TransferItem(int transferId, TransferDirection dir,
                          const QString& fileName, QWidget* parent = nullptr);

    void setProgress(qint64 current, qint64 total);
    void setFinished();
    void setInterrupt();

    int transferId() const { return m_transferId; }

signals:
    void cancelled(int transferId);       // 传输中点击取消
    void closeRequested(int transferId);  // 完成后点击关闭

private slots:
    void onCancelClicked();

private:
    int m_transferId;
    bool m_isFinished = false;

    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_percentLabel;
    QProgressBar* m_progressBar;
    QLabel* m_detailLabel;
    QPushButton* m_cancelBtn;
};

// ========== 多项目进度对话框 ==========
class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget* parent = nullptr);

    int addItem(TransferDirection dir, const QString& fileName);
    void removeItem(int transferId);
    TransferItem* item(int transferId) const;
    int activeCount() const { return m_activeCount; }

signals:
    void cancelled(int transferId);
    void allTransfersFinished();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QScrollArea* m_scrollArea;
    QWidget* m_container;
    QVBoxLayout* m_itemLayout;
    QMap<int, TransferItem*> m_items;
    int m_nextId = 0;
    int m_activeCount = 0;

    void onItemFinished();
};

#endif // PROGRESSDIALOG_H
