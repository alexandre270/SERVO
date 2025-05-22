#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "client.h"
#include "server.h"
#include "camera.h"
#include "dbmanager.h"
#include "videostream.h"

#include <QDebug>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QHostAddress>
#include <QTcpSocket>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpServer(nullptr)
    , clientSocket(nullptr)
{
    qDebug() << "💡 Début MainWindow";

    ui->setupUi(this);
    qDebug() << "✅ UI chargé";

    // Label de commande
    commandLabel = new QLabel(this);
    commandLabel->setObjectName("commandLabel");
    ui->verticalLayout->addWidget(commandLabel);

    // Objets principaux
    client = new Client(this);
    server = new Server(client, this);
    dbManager = new DBManager(this);
    camera = new Camera(this);
    qDebug() << "✅ Objets instanciés";

    connect(client, &Client::sensorDataReceived, server, &Server::archiveSensorData);
    qDebug() << "✅ Signal capteur connecté";

    // Démarrer serveur TCP
    startServer();
    qDebug() << "✅ Serveur TCP démarré";

    // Flux vidéo sécurisé
    try {
        videoStream = new VideoStream("udp://192.168.0.21:1234", this);

        connect(videoStream, &VideoStream::frameReady, this, [=](const QImage &img) {
            if (!img.isNull()) {
                updateFrame(img);
                client->handleVideoFrame(img);
            } else {
                qDebug() << "⚠️ Image vidéo invalide ignorée.";
            }
        });

        videoStream->start();
        qDebug() << "✅ Flux vidéo actif";
    } catch (...) {
        qDebug() << "❌ Erreur lors du démarrage du flux vidéo.";
    }

    // Utilise les widgets définis dans l'interface UI
    ui->servoSlider->setRange(0, 180);
    ui->servoSlider->setValue(90);

    connect(ui->sendServoButton, &QPushButton::clicked, this, [=]() {
        int angle = ui->servoSlider->value();
        QString message = QString::number(angle) + "\n";

        QTcpSocket *servoSocket = new QTcpSocket(this);
        servoSocket->connectToHost(QHostAddress("192.168.1.42"), 12345);

        if (servoSocket->waitForConnected(2000)) {
            servoSocket->write(message.toUtf8());
            servoSocket->waitForBytesWritten();
            servoSocket->close();
            commandLabel->setText("Angle envoyé : " + QString::number(angle));
            qDebug() << "✅ Angle envoyé au servomoteur :" << angle;
        } else {
            commandLabel->setText("Erreur : Servo non joignable !");
            qDebug() << "❌ Connexion échouée vers servo.";
        }

        servoSocket->deleteLater();
    });

    qDebug() << "✅ Initialisation complète MainWindow";
}

MainWindow::~MainWindow()
{
    if (tcpServer) {
        tcpServer->close();
        tcpServer->deleteLater();
    }

    delete videoStream;
    delete camera;
    delete server;
    delete client;
    delete dbManager;
    delete ui;
}

void MainWindow::startServer()
{
    tcpServer = new QTcpServer(this);

    connect(tcpServer, &QTcpServer::newConnection, this, [=]() {
        clientSocket = tcpServer->nextPendingConnection();
        qDebug() << "Client connecté depuis" << clientSocket->peerAddress().toString();

        connect(clientSocket, &QTcpSocket::readyRead, this, [=]() {
            QByteArray data = clientSocket->readAll();
            qDebug() << "Données reçues du client Flutter:" << QString(data);
        });

        connect(clientSocket, &QTcpSocket::disconnected, this, [=]() {
            qDebug() << "Client déconnecté.";
            clientSocket->deleteLater();
            clientSocket = nullptr;
        });
    });

    if (!tcpServer->listen(QHostAddress::Any, 12346)) {
        qDebug() << "Erreur lors du démarrage du serveur:" << tcpServer->errorString();
    } else {
        qDebug() << "Serveur en écoute sur le port 12346";
    }
}

void MainWindow::on_sendButton_clicked()
{
    QString message = ui->messageLineEdit->text();
    client->sendMovementCommand(message);
    ui->messagesTextEdit->append("Commande envoyée: " + message);
    commandLabel->setText("Commande: " + message);
}

void MainWindow::on_moveButton_clicked()
{
    QString command = ui->commandLineEdit->text();
    client->sendMovementCommand(command);
    ui->commandsTextEdit->append("Commande de mouvement envoyée: " + command);
    commandLabel->setText("Commande: " + command);
}

void MainWindow::on_fetchDataButton_clicked()
{
    QStringList data = dbManager->fetchData();
    ui->measurementsTextEdit->clear();
    for (const QString &item : data) {
        ui->measurementsTextEdit->append(item);
    }
}

void MainWindow::on_captureScreenButton_clicked()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap screenshot = screen->grabWindow(0);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("capture_%1.png").arg(timestamp);

    if (screenshot.save(filename)) {
        dbManager->insertSensorData("Capture d'écran", filename);
        ui->commandsTextEdit->append("Capture d'écran sauvegardée: " + filename);
    } else {
        qDebug() << "Erreur lors de la sauvegarde de la capture d'écran.";
    }
}

void MainWindow::updateFrame(const QImage &image)
{
    ui->videoLabel->setPixmap(QPixmap::fromImage(image));
}

void MainWindow::updateMeasurements(const QString &measurements)
{
    ui->measurementsTextEdit->setPlainText(measurements);
}
