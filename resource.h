#ifndef RESOURCE_H
#define RESOURCE_H

#include <QTcpSocket>
#include <QTime>

class Resource
{
public:
    Resource();
    virtual ~Resource();

    void setNewOwner(QTcpSocket *p_socket, int time, QString name);
    void setOwnerSocket(QTcpSocket *ownerSocket);
    void setOwnerName(const QString &ownerName);
    QTcpSocket *getOwnerSocket() const;
    int getCaptureTime() const;
    void setCaptureTime(const int &captureTime);
    QString getOwnerName() const;

    // \brief Освободить ресурс
    void freeRecource();

private:
    QTcpSocket *m_ownerSocket = nullptr;
    int m_captureTime = 0;
    QString m_ownerName = "";


};

#endif // RESOURCE_H
