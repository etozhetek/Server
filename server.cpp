#include "server.h"
#include <QtCore>

Server::Server()
{
    connect(&m_server, &QTcpServer::newConnection, this, &Server::slotNewConnection);
    init();
}

Server::~Server()
{
    if (m_server.isListening())
    {
        m_server.close();
    }
    // записываем в файл настройки, измененные во время работы
    writeSettings();
}

void Server::setMaxCaptureTime(const int& maxCaptureTime)
{
    m_maxCaptureTime = maxCaptureTime;
}

void Server::setDeclineNewResource(bool value)
{
    m_declineNewResource = value;
}

void Server::setAccepting(const bool &value)
{
    if (value){
        m_server.pauseAccepting();
    }else {
        m_server.resumeAccepting();
    }
}

qint16 Server::getPort()
{
    return  m_server.serverPort();
}

QString Server::getResourceOwnerUsername(const qint32 &resourceNumber) const
{
    return m_resourcesVec[resourceNumber - 1].getOwnerName();
}

QTime Server::getCapturedResourceDuration(const qint32 &resource) const
{
    QTime duration = QTime(0,0);
    int durationInSeconds = ((QTime::currentTime().msecsSinceStartOfDay() / 1000)
                             - m_resourcesVec[resource - 1].getCaptureTime());
    return duration.addSecs(durationInSeconds);
}

QDateTime Server::getServerStartTime() const
{
    return m_serverStartTime;
}

QVector<Resource> Server::getResourcesVec()
{
    return m_resourcesVec;
}

int Server::getMaxCaptureTime() const
{
    return m_maxCaptureTime;
}

int Server::getConnectionsCounter() const
{
    return m_connectionsCounter;
}

QMap<QString, QTcpSocket *> Server::getConnectionsUsersMap() const
{
    return m_connectionsUsersMap;
}

void Server::writeSettings()
{
    m_settings->setValue("MaxCaptureTime", m_maxCaptureTime);
}

void Server::freeAllResources()
{
    // Проверяем все ресурсы на наличие обладателя, и освобождаем
    for (int i = 0;i < m_resourcesVec.size();i++) {
        if (!m_resourcesVec[i].getOwnerName().isEmpty()){
            creatingResponseToRequest(m_resourcesVec[i].getOwnerName(), i, 0);
            m_resourcesVec[i].freeRecource();
            emit signalChangeResourceOwner(i);
        }
    }
}

void Server::slotNewConnection()
{
    QTcpSocket *p_socket = m_server.nextPendingConnection();

    // Проверка, не достигло ли количество подключенных клиентов максимума
    if (m_connectionsCounter == m_maxConnectionsAmount){
        p_socket->deleteLater();
        p_socket->abort();
        return;
    }

    m_connectionsCounter++;

    connect(p_socket, &QTcpSocket::readyRead, this, &Server::slotReadyRead);
    connect(p_socket, &QTcpSocket::disconnected, this, &Server::slotDisconnected);
}

void Server::slotReadyRead()
{
    QTcpSocket *p_clientSocket = (QTcpSocket*)sender();

    // Проверяем не в бане ли клиент
    if (m_banSet.contains(p_clientSocket->peerAddress())){
        p_clientSocket->abort();
        return;
    }

    /*Тк нет клиентской части, и ничего про нее несказано, то реализовал так,
        первые 4 байта посылки - это длина полезных данных, то есть, считываем
        сначала длину полезных данных, создаем массив соответсвующей длины, и туда
        помещаем данные из потока*/
    QDataStream in(p_clientSocket);
    in.setVersion(QDataStream::Qt_5_12);
    in >> m_nextBlockSize;
    m_buffArray.resize(m_nextBlockSize);
    in.readRawData(m_buffArray.data(), m_nextBlockSize);


    auto jDoc = QJsonDocument::fromJson(m_buffArray, &jErrors);
    // Проверяем валидность JSON'a
    if (jErrors.error == QJsonParseError::NoError){
        readJSON(jDoc.object(), p_clientSocket);
    }

    m_nextBlockSize = 0;
    m_buffArray.clear();
}

void Server::slotDisconnected()
{
    QTcpSocket *p_clientSocket = (QTcpSocket*)sender();

    // Получаем имя отключаемого клиента
    QString keyUsername = m_connectionsUsersMap.key(p_clientSocket);

    // Проверяем, закреплен ли за отключаемым клиентом ресурс, если да, то освобождаем его
    for (int i = 0;i < m_resourcesVec.size();i++){
        if (p_clientSocket == m_resourcesVec[i].getOwnerSocket()){
            emit signalChangeResourceOwner(i);
            m_resourcesVec[i].freeRecource();
        }
    }

    if (!keyUsername.isEmpty()){
        m_connectionsUsersMap.remove(keyUsername);
        emit signalRemoveUser(keyUsername);
    }

    p_clientSocket->deleteLater();
    m_connectionsCounter--;
}

void Server::init()
{
    // Считываем настройки из файла
    readSettings();
    // Начинаем слушать порт
    if (!m_server.listen(QHostAddress::Any, m_port))
    {
        qDebug() << "Error server create";
        m_server.close();
    } else
    {
        qDebug() << "Server started";
    }

    // Устанавливаем время старата сервера
    m_serverStartTime = QDateTime::currentDateTime();
    // Создаем 4 ресурса в массиве
    m_resourcesVec.resize(4);
}

void Server::readSettings()
{
    m_settings = new QSettings(QDir::currentPath() + "/settings.conf", QSettings::IniFormat);

    m_port = m_settings->value("Port", 1234).toInt();
    m_maxCaptureTime = 3600 * m_settings->value("MaxCaptureTime", 2).toInt();
    m_maxConnectionsAmount = m_settings->value("Max_Users_Amount", 20).toInt();

    int size = m_settings->beginReadArray("Usernames");

    for (int i = 0;i < size;i++){
        m_settings->setArrayIndex(i);
        m_usernamesSet.insert(m_settings->value("user").toString());
    }

    m_settings->endArray();
}

void Server::readJSON(const QJsonObject& jObj, QTcpSocket* p_clientSocket)
{
    // Если не содержит поле "username", то JSON не подходит
    if (!jObj.contains("username")){
        return;
    }

    int keysAmount = jObj.keys().size();
    QString username = jObj.value("username").toString();

    // Проверка на бан
    if (!m_usernamesSet.contains(username)){
        m_banSet.insert(p_clientSocket->peerAddress());
        p_clientSocket->abort();
        return;
    }

    // Если ключ 1, следовательно, это точнo запрос авторизации
    if (keysAmount == 1){
        if (!m_connectionsUsersMap.contains(username)){
            m_connectionsUsersMap.insert(username, p_clientSocket);
            emit signalAddNewUser(username);
        }
        return;
    }

    // Если ключа 3, и 2 из оставшихся "request" и "time", то парсим его
    if (jObj.contains("time") && jObj.contains("request")
            && m_connectionsUsersMap.keys().contains(username) && keysAmount == 3){
        captureResources(username,jObj.value("request").toInt(), jObj.value("time").toInt());
        return;
    }
}

void Server::creatingResponseToRequest(const QString username, const int resource, const char status)
{
    QJsonObject jObj {{"username", username}, {"resource", resource + 1}, {"status", status}};

    sendToClient(QJsonDocument(jObj).toJson(), username);
}

void Server::captureResources(const QString username, const int request, const int newCaptureTime)
{
    // проверяем, что ресурс вообще запрошен
    if (request == 0){
        return;
    }

    int oldResource = -1; // Переменная для ресурса, захваченного ранее (если такой есть)
    int newResource = -1; // Переменная для номера нового ресурса

    // Перебираем ресурсы, чтобы узнать какой требуется, и был ли захвачен какой-либо ранее
    for(int i = 0;i < 4;i++){
        // Проверяем не перезапрашивает ли пользователь захваченный им же ресурс
        if (m_resourcesVec[i].getOwnerName() == username && username != ""){
            oldResource = i;
        }

        // Проверяем, какой ресурс запрашивает пользователь
        if (request >> i*8 & 0xFF){

            // Проверяем, разрешен ли захват, если нет, то высылаем отказ о выдаче
            if (m_declineNewResource){
                creatingResponseToRequest(username, i, 0);
                return;
            }

            // Проверяем имеет ли ресурс обладателя или нет, если нет, то выдаем его
            if (m_resourcesVec[i].getOwnerSocket() == nullptr){
                newResource = i;
                continue;
            }

            // Если ресурс имеет обладателя, проверям время владения, если больше
            // заданного, то передаем ресурс, если нет то высылаем отказ
            if (newCaptureTime - m_resourcesVec[i].getCaptureTime() > m_maxCaptureTime
                    && m_resourcesVec[i].getOwnerName() != username){
                creatingResponseToRequest(m_resourcesVec[i].getOwnerName(), i, 0);
                m_resourcesVec[i].freeRecource();
                emit signalChangeResourceOwner(i);
                newResource = i;
                continue;
            } else{
                creatingResponseToRequest(username, i, 0);
                return;
            }
        }
    }

    // если был ранее захвачен ресурс, то освобождаем его
    if (oldResource != - 1){
        creatingResponseToRequest(m_resourcesVec[oldResource].getOwnerName(), oldResource, 0);
        m_resourcesVec[oldResource].freeRecource();
        emit signalChangeResourceOwner(oldResource);
    }

    // Закрепляем новый ресурс за пользователем
    m_resourcesVec[newResource].setNewOwner(m_connectionsUsersMap.value(username), newCaptureTime, username);
    creatingResponseToRequest(username, newResource, 1);
    emit signalChangeResourceOwner(newResource, username);
}

void Server::sendToClient(const QByteArray& payload, const QString &username)
{
    // Проверяем состояние сокета, если все ок, то высылаем всю информацию
    if(m_connectionsUsersMap.value(username)->isOpen()){
        QByteArray  arrBlock;
        QDataStream out(&arrBlock, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_12);
        out << payload;
        m_connectionsUsersMap.value(username)->write(arrBlock);
        m_connectionsUsersMap.value(username)->flush();
    }
}
