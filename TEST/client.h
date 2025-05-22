#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QImage>

class Client : public QObject {
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr);
    void sendMovementCommand(const QString &command);
    void sendDataToAndroid(const QString &data);
    void handleVideoFrame(const QImage &image);

signals:
    void sensorDataReceived(const QString &sensorType, float value);

private:
    QTcpSocket *robotSocket;
    QTcpSocket *flutterSocket;
    QTcpServer *server;
    bool socketWasConnected = true;

    char mapCommande(const QString &msg);
    void readClientData();
};

#endif // CLIENT_H
