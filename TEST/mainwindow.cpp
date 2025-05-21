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
    ui->setupUi(this);

    // Label pour afficher les commandes
    commandLabel = new QLabel(this);
    commandLabel->setObjectName("commandLabel");
    ui->verticalLayout->addWidget(commandLabel);

    // Instanciation
    client = new Client(this);
    server = new Server(client, this);
    dbManager = new DBManager(this);
    camera = new Camera(this);

    // Connexion des capteurs
    connect(client, &Client::sensorDataReceived, server, &Server::archiveSensorData);

    // Serveur TCP
    startServer();

    // Flux vidéo sécurisé
    videoStream = new VideoStream("udp://192.168.0.21:1234", this);
    connect(videoStream, &VideoStream::frameReady, client, &Client::handleVideoFrame);
    connect(videoStream, &VideoStream::frameReady, this, &MainWindow::updateFrame);
    videoStream->start();

    // === Contrôle du servomoteur ===
    servoSlider = new QSlider(Qt::Horizontal, this);
    servoSlider->setRange(0, 180);
    servoSlider->setValue(90);
    ui->verticalLayout->addWidget(servoSlider);

    sendServoButton = new QPushButton("Envoyer angle au servomoteur", this);
    ui->verticalLayout->addWidget(sendServoButton);

    connect(sendServoButton, &QPushButton::clicked, this, [=]() {
        int angle = servoSlider->value();
        QString message = QString::number(angle) + "\n";

        QTcpSocket *servoSocket = new QTcpSocket(this);
        servoSocket->connectToHost(QHostAddress("192.168.1.42"), 12345); // ⚠️ Adresse IP à adapter

        if (servoSocket->waitForConnected(2000)) {
            servoSocket->write(message.toUtf8());
            servoSocket->flush();
            servoSocket->waitForBytesWritten();
            servoSocket->close();
            commandLabel->setText("Angle envoyé : " + QString::number(angle));
        } else {
            commandLabel->setText("Erreur : Servo non joignable !");
        }

        servoSocket->deleteLater();
    });

    // Boutons
    connect(ui->moveUpButton, &QPushButton::clicked, this, &MainWindow::moveUp);
    connect(ui->moveDownButton, &QPushButton::clicked, this, &MainWindow::moveDown);
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

void MainWindow::moveUp()
{
    client->sendMovementCommand("MOVE_UP");
    commandLabel->setText("Commande: MOVE_UP");
}

void MainWindow::moveDown()
{
    client->sendMovementCommand("MOVE_DOWN");
    commandLabel->setText("Commande: MOVE_DOWN");
}
