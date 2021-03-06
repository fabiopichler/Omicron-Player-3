/*******************************************************************************

  Omicron Player Classic

  Author: Fábio Pichler
  Website: http://fabiopichler.net
  License: BSD 3-Clause License

  Copyright (c) 2015-2019, Fábio Pichler
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Omicron Player Classic nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "Update.h"
#include "../Core/Database.h"

#include <OmicronTK/Qt/AppInfo.hpp>

#include <iostream>

#include <QFileInfo>
#include <QProcess>

using namespace OmicronTK;

UpdateApp::UpdateApp(QObject *parent, QSettings *iniSettings) : QObject(parent)
{
    this->iniSettings = iniSettings;
    blockRequest = false;
    playlistFile = nullptr;
    currentDate = QDate::currentDate();

    updateManager = new QNetworkAccessManager(this);
    downloadManager = new QNetworkAccessManager(this);
    startTimer = new QTimer(this);
    startTimer->setSingleShot(true);

    connect(updateManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(finishedChecking(QNetworkReply*)));
    connect(startTimer, SIGNAL(timeout()), this, SLOT(startChecking()));
    connect(this, SIGNAL(showNotification(QString)), parent, SLOT(showNotification(QString)));

    int checkUpdate = Database::value("Version", "updates_check", 1).toInt();

    if (checkUpdate == 0)
        return;
    else if (checkUpdate == 2)
        checkUpdate = 7;
    else if (checkUpdate != 1)
        checkUpdate = 1;

    QDate lastCheck(QDate::fromString(Database::value("Version", "updates_lastCheck").toString(), "yyyy-MM-dd"));
    lastCheck = lastCheck.addDays(checkUpdate);

    if (lastCheck <= currentDate)
        startTimer->start(1000);
}

UpdateApp::~UpdateApp()
{
    requestAborted = true;

    delete updateManager;
    delete downloadManager;
    delete startTimer;

    if (playlistFile)
    {
        playlistFile->close();
        playlistFile->remove();
        delete playlistFile;
    }
}

//================================================================================================================
// public slots
//================================================================================================================

void UpdateApp::startChecking(const bool &arg)
{
    stdCout("Checking for updates...");

    if (arg)
        emit showNotification("Verificando por atualizações. Aguarde...");

    alert = arg;
    QString p("?version="+CurrentVersion);

    QNetworkRequest request;
    request.setUrl(QUrl((newURL.isEmpty() ? CheckUpdate+p : (newURL.contains("version=") ? newURL : newURL+p))));
    request.setRawHeader("User-Agent", QString(AppNameId+"-"+CurrentVersion).toLocal8Bit());

    updateManager->get(request);
}

//================================================================================================================
// private
//================================================================================================================

void UpdateApp::downloadPlaylist(const QUrl &url)
{
    QString fileName = "RadioList_" + newPlaylistDate + ".7z";

    playlistFile = new QFile(Global::getConfigPath(fileName));

    if (!playlistFile->open(QIODevice::WriteOnly))
    {
        qDebug() << "Error: " << playlistFile->errorString();
        delete playlistFile;
        playlistFile = nullptr;
        return;
    }

    requestAborted = false;
    blockRequest = true;

    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", QString(AppNameId+"-"+CurrentVersion).toLocal8Bit());

    downloadReply = downloadManager->get(request);

    connect(downloadReply, SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(downloadReply, SIGNAL(finished()), this, SLOT(downloadFinished()));
}

//================================================================================================================
// private slots
//================================================================================================================

void UpdateApp::readyRead()
{
    if (playlistFile)
        playlistFile->write(downloadReply->readAll());
}

void UpdateApp::finishedChecking(QNetworkReply *reply)
{
    int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();

    if (code == 200)
    {
        QJsonObject json = QJsonDocument::fromJson(data).object();
        QJsonObject app = json.value("app").toObject();
        QJsonObject radioPlaylist = json.value("radioPlaylist").toObject();

        QString version = app["currentVersion"].toString();
        newPlaylistName = radioPlaylist["fileName"].toString();
        newPlaylistDate = radioPlaylist["date"].toString();
        QUrl radiolistUrl = radioPlaylist["url"].toString();

        if (!version.isEmpty())
        {
            stdCout("Checking update is completed");
            Database::setValue("Version", "updates_lastCheck", currentDate.toString("yyyy-MM-dd"));

            QDate listInstalled(QDate::fromString(iniSettings->value("Radiolist/Date").toString(), "yyyy-MM-dd"));
            QDate dateChecked(QDate::fromString(newPlaylistDate, "yyyy-MM-dd"));

            if (!blockRequest && Database::value("Config", "autoDlRadioList").toBool()
                                                 && !radiolistUrl.isEmpty() && listInstalled < dateChecked)
                downloadPlaylist(radiolistUrl);

            if (version == CurrentVersion && alert)
            {
                QMessageBox::information(nullptr,"Info",QString("<h3>O %1 está atualizado.</h3>Versão atual: %2").arg(AppName,version));
            }
            else if (version != CurrentVersion)
            {
                stdCout("New version available: " + version + "\n");

                if (QMessageBox::warning(nullptr,"Info",QString("<h2>Nova versão disponível</h2>Uma nova versão do <strong>%1</strong> "
                                                           "está disponível,<br>é altamente recomendável que instale a atualização,"
                                                           "<br>pois contém várias melhorias e correções de bugs.<br><br>"
                                                           "<strong>Versão instalada:</strong> %2<br>"
                                                           "<strong>Nova versão:</strong> %3<br><br>Deseja acessar a internet e "
                                                           "baixar a última versão?").arg(AppName,CurrentVersion,version),
                                                           QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
                    QDesktopServices::openUrl(QUrl(DownloadUpdate));
            }

            reply->deleteLater();
            return;
        }
    }
    else if (code == 301 || code == 302)
    {
        newURL = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
        startChecking(alert);
        reply->deleteLater();
        return;
    }

    stdCout("Unable to check for updates. Code: " + QString::number(code));
    reply->deleteLater();

    if (alert)
        QMessageBox::warning(nullptr,"Info",QString("Não foi possível verificar se há atualizações.\nVerifique sua conexão à "
                                               "internet e tente novamente.\nCódigo do erro: %1").arg(code));
}

void UpdateApp::downloadFinished()
{
    int code = downloadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    playlistFile->flush();
    playlistFile->close();

    if (!requestAborted && downloadReply->error() == QNetworkReply::NoError)
    {
        QString args("\"");

        args.append(OTKQT::AppInfo::pluginsPath())
                .append("/7-Zip/7zr\" x -y \"")
                .append(playlistFile->fileName())
                .append("\" -o\"")
                .append(Global::getConfigPath())
                .append("\"");

        if (QProcess::execute(args) == 0)
        {
            iniSettings->setValue("Radiolist/NewFileName", newPlaylistName);
            iniSettings->setValue("Radiolist/NewDate", newPlaylistDate);
            emit showNotification("A Lista de Rádios foi atualizada e\nserá ativada, após reiniciar o programa.");
        }
        else
        {
            qDebug() << "Erro ao Extrair o novo RadioList";
        }
    }

    downloadReply->deleteLater();
    playlistFile->remove();
    delete playlistFile;
    playlistFile = nullptr;

    if (!requestAborted && (code == 301 || code == 302))
    {
        downloadPlaylist(downloadReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
        return;
    }

    blockRequest = false;
}
