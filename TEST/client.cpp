#include "client.h"
#include <QBuffer>
#include <QDebug>
#include <QStringList>

Client::Client(QObject *parent) : QObject(parent) {
    robotSocket = new QTcpSocket(this);
    robotSocket->connectToHost("192.168.0.133", 12345);
    if (!robotSocket->waitForConnected(3000)) {
        qDebug() << "Erreur de connexion au robot:" << robotSocket->errorString();
    } else {
        qDebug() << "Client connecté au robot.";
    }

    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, [this]() {
        flutterSocket = server->nextPendingConnection();
        qDebug() << "Client Flutter connecté !";
        connect(flutterSocket, &QTcpSocket::readyRead, this, &Client::readClientData);
    });

    if (!server->listen(QHostAddress::Any, 12345)) {
        qDebug() << "Erreur d'écoute :" << server->errorString();
    } else {
        qDebug() << "Serveur en écoute sur le port 12345";
    }

    connect(robotSocket, &QTcpSocket::readyRead, this, [this]() {
        QByteArray data = robotSocket->readAll();
        QStringList parts = QString(data).split(":");
        if (parts.size() == 2) {
            QString sensorType = parts[0];
            float value = parts[1].toFloat();
            emit sensorDataReceived(sensorType, value);
        }
    });
}

char Client::mapCommande(const QString &msg) {
    if (msg == "move_forward") return 'Z';
    if (msg == "move_backward") return 'S';
    if (msg == "move_forward_left") return 'Q';
    if (msg == "move_forward_right") return 'D';
    if (msg == "stop") return 'X';
    if (msg == "rotation") return 'A';
    if (msg == "move_backward_left") return 'W';
    if (msg == "move_backward_right") return 'C';
    return 'X';
}

void Client::sendMovementCommand(const QString &command) {
    if (robotSocket->state() == QAbstractSocket::ConnectedState) {
        robotSocket->write(command.toUtf8());
        robotSocket->flush();
        qDebug() << "Commande envoyée au robot:" << command;
    } else {
        qDebug() << "Le client n'est pas connecté au robot.";
    }
}

void Client::readClientData() {
    if (!flutterSocket) return;

    QByteArray data = flutterSocket->readAll();
    QString message = QString(data).trimmed();
    qDebug() << "Reçu de Flutter:" << message;

    if (message != "connect" && message != "disconnect") {
        if (robotSocket && robotSocket->isOpen()) {
            char commande = mapCommande(message);
            robotSocket->write(&commande, 1);
            robotSocket->flush();
            qDebug() << "Envoyé au robot:" << commande;
        } else {
            qDebug() << "robotSocket non connecté.";
        }
    }

    QString response = "OK:" + message + "\n";
    flutterSocket->write(response.toUtf8());
    flutterSocket->flush();
}

void Client::sendDataToAndroid(const QString &data) {
    if (flutterSocket && flutterSocket->state() == QAbstractSocket::ConnectedState) {
        flutterSocket->write(data.toUtf8());
        flutterSocket->flush();
        qDebug() << "Données envoyées à Flutter:" << data;
    } else {
        qDebug() << "Flutter n'est pas connecté.";
    }
}

void Client::handleVideoFrame(const QImage &image) {
    if (flutterSocket && flutterSocket->state() == QAbstractSocket::ConnectedState) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        image.save(&buffer, "JPEG");
        QString base64Image = QString::fromLatin1(byteArray.toBase64());
        QString message = "{\"video\":\"" + base64Image + "\"}\n";
        qDebug() << "Envoi image vers Flutter: " << message.left(60) << "...";
        flutterSocket->write(message.toUtf8());
        flutterSocket->flush();
    } else {
        if (socketWasConnected) {
            qDebug() << "Socket non connecté, image ignorée.";
            socketWasConnected = false;
        }
    }
}
