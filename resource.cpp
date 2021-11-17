#include "resource.h"

QTcpSocket *Resource::getOwnerSocket() const
{
    return m_ownerSocket;
}

void Resource::setOwnerSocket(QTcpSocket *ownerSocket)
{
    m_ownerSocket = ownerSocket;
}

int Resource::getCaptureTime() const
{
    return m_captureTime;
}

void Resource::setCaptureTime(const int &captureTime)
{
    m_captureTime = captureTime;
}

QString Resource::getOwnerName() const
{
    return m_ownerName;
}

void Resource::setOwnerName(const QString &ownerName)
{
    m_ownerName = ownerName;
}

void Resource::freeRecource()
{
    m_ownerName = "";
    m_captureTime = 0;
    m_ownerSocket = nullptr;
}

void Resource::setNewOwner(QTcpSocket *p_socket, int time, QString name)
{
    m_ownerSocket = p_socket;
    m_captureTime = time;
    m_ownerName = name;
}

Resource::Resource()
{

}

Resource::~Resource()
{
    delete  m_ownerSocket;
}
