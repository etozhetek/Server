#ifndef SERVER_H
#define SERVER_H

#include "Server_global.h"
#include <QTcpServer>
#include <QJsonParseError>
#include <QDateTime>
#include <QSettings>
#include "resource.h"

class SERVER_EXPORT Server : public QObject
{
    Q_OBJECT

public:
    Server();
    virtual ~Server();

    //! \brief  Установка максимального времени захвата ресурса
    void setMaxCaptureTime(const int &maxCaptureTime);
    //! \brief Установка режима отклонения запросов новых ресурсов
    void setDeclineNewResource(bool value);
    //! \brief Установка режима отклонения новых входящих подключений
    void setAccepting(const bool &value);
    //! \brief Получить номер прослушиваемого порта
    qint16 getPort();
    //! \brief Получить имя обладателя N-ого ресурса
    QString getResourceOwnerUsername(const qint32& resourceNumber) const;
    //! \brief Получить текущее время владения пользователя ресурсом
    QTime getCapturedResourceDuration(const qint32& resource) const;
    //! \brief Получить время начала работы серврера
    QDateTime getServerStartTime() const;
    //! \brief Получить список ресурсов
    QVector<Resource> getResourcesVec();
    //! \brief Получить максимальную длительность владения ресурсом, до того как другой пользователь сможет перехватить ресурс
    int getMaxCaptureTime() const;
    //! \brief Получить количество подключенныз клиентов (не авторизированных!)
    int getConnectionsCounter() const;
    //! \brief Получить карту: имя пользователя - сокет
    QMap<QString, QTcpSocket *> getConnectionsUsersMap() const;
    //! \brief Запись изменений настроек в файл
    void writeSettings();
    //! \brief Освободить все ресурсы
    void freeAllResources();


signals:
    //! \brief Сигнализирует об авторизации нового пользователя
    void signalAddNewUser(const QString& username);
    //! \brief Сигнализирует об отключении пользователя
    void signalRemoveUser(const QString& username);
    //! \brief Сигнализирует о смене владельца ресуром
    void signalChangeResourceOwner(const qint32& resource, const QString& username = "");

private slots:
    void slotNewConnection();
    void slotReadyRead();
    void slotDisconnected();

private:
    //! \brief Инициализация сервреа
    void init();
    //! \brief Чтение настроек
    void readSettings();
    //! \brief парсинг JSON'a
    void readJSON(const QJsonObject &jObj, QTcpSocket *socket);
    //! \brief Создание JSON'a для ответа клиенту
    void creatingResponseToRequest(const QString username, const int resource, const char status);
    //! \brief Непосредственно захват ресурса
    void captureResources(const QString username, const int request, const int newCaptureTime);
    //! \brief Отправка клиенту
    void sendToClient(const QByteArray &payload, const QString &username);

private:
    bool                       m_declineNewResource = false; ///< Флаг, для выдачи новых ресурсов
    qint16                     m_port;
    qint32                     m_connectionsCounter = 0;
    qint32                     m_maxConnectionsAmount; ///< Максимальное количество подключенных клиентов
    qint32                     m_maxCaptureTime; ///< Максимальное время захвата
    quint32                    m_nextBlockSize = 0;
    QJsonParseError            jErrors;
    QDateTime                  m_serverStartTime; ///< Время запуска сервреа
    QTcpServer                 m_server;
    QSettings                  *m_settings; ///< Файл настроек
    QByteArray                 m_buffArray;
    QVector<Resource>          m_resourcesVec; ///< Массив ресурсов
    QSet <QString>             m_usernamesSet; ///< Список имен, которым разрешен доступ
    QSet <QHostAddress>        m_banSet; ///< Бан лист
    QMap<QString, QTcpSocket *>m_connectionsUsersMap; ///< Карта имя пользователя - сокет
};

#endif // SERVER_H
