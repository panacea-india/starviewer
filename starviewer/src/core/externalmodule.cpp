/*************************************************************************************
  Copyright (C) 2014 Laboratori de Gràfics i Imatge, Universitat de Girona &
  Institut de Diagnòstic per la Imatge.
  Girona 2014. All rights reserved.
  http://starviewer.udg.edu

  This file is part of the Starviewer (Medical Imaging Software) open source project.
  It is subject to the license terms in the LICENSE file found in the top-level
  directory of this distribution and at http://starviewer.udg.edu/license. No part of
  the Starviewer (Medical Imaging Software) open source project, including this file,
  may be copied, modified, propagated, or distributed except according to the
  terms contained in the LICENSE file.
 *************************************************************************************/

#include "externalmodule.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>

namespace udg {

namespace {

const int MaxLineLength = 1024 * 1024;
const int MaxMessages = 20000;
const int MaxOutputs = 1000;

// Stops the module and what it started: on Windows a module started through a script (.cmd) has a child that is not stopped by killing the script
void killTree(QProcess &process)
{
#ifdef Q_OS_WIN
    if (process.state() != QProcess::NotRunning)
    {
        QProcess::execute("taskkill", QStringList() << "/F" << "/T" << "/PID" << QString::number(process.processId()));
    }
#endif
    if (process.state() != QProcess::NotRunning)
    {
        process.kill();
    }

    process.waitForFinished(3000);
}

QString canonical(const QString &path)
{
    QFileInfo info(path);

    // The part of the path that exists is resolved (links too); the rest is added as it is
    QString existing = info.absoluteFilePath();
    QString rest;

    while (!QFileInfo::exists(existing) && !QFileInfo(existing).isRoot())
    {
        QFileInfo parent(existing);
        rest = QString("/") + parent.fileName() + rest;
        existing = parent.absolutePath();
    }

    QString resolved = QFileInfo(existing).canonicalFilePath();
    return QDir::cleanPath((resolved.isEmpty() ? existing : resolved) + rest);
}

}

QString ModuleManifest::executablePath() const
{
    return QDir::cleanPath(QDir(directory).absoluteFilePath(executable));
}

bool ExternalModules::isInside(const QString &path, const QString &folder)
{
    QString p = canonical(path);
    QString f = canonical(folder);
    return p.compare(f, Qt::CaseInsensitive) == 0 || (p + "/").startsWith(f + "/", Qt::CaseInsensitive);
}

bool ExternalModules::parseManifest(const QJsonObject &object, ModuleManifest *manifest, QStringList *problems)
{
    QStringList found;
    ModuleManifest m;
    m.name = object.value("name").toString().trimmed();
    m.version = object.value("version").toString().trimmed();
    m.description = object.value("description").toString();
    m.executable = object.value("executable").toString().trimmed();
    m.researchOnly = object.contains("researchOnly") ? object.value("researchOnly").toBool(true) : true;
    m.network = object.contains("network") ? object.value("network").toString("none") : QString("none");
    m.timeoutSeconds = object.contains("timeoutSeconds") ? object.value("timeoutSeconds").toInt(0) : 600;

    for (const QJsonValue &v : object.value("arguments").toArray())
    {
        m.arguments << v.toString();
    }

    if (object.value("protocol").toInt(0) != 1)
    {
        found << "The manifest must have \"protocol\": 1.";
    }

    if (m.name.isEmpty())
    {
        found << "The manifest has no name.";
    }

    if (m.version.isEmpty())
    {
        found << "The manifest has no version.";
    }

    if (m.executable.isEmpty() || QDir::isAbsolutePath(m.executable) || m.executable.contains("..") || m.executable.startsWith('/') || m.executable.startsWith('\\') || m.executable.contains(':'))
    {
        found << "The executable must be a path inside the folder of the module (not absolute and with no \"..\").";
    }

    if (m.timeoutSeconds < 1 || m.timeoutSeconds > 86400)
    {
        found << "The timeout is from 1 to 86400 seconds.";
    }

    if (m.network.trimmed().isEmpty())
    {
        found << "\"network\" says \"none\" or what the module connects to.";
    }

    if (!found.isEmpty())
    {
        if (problems)
        {
            *problems = found;
        }

        return false;
    }

    *manifest = m;
    return true;
}

bool ExternalModules::loadManifest(const QString &directory, ModuleManifest *manifest, QStringList *problems)
{
    QFile file(QDir(directory).filePath("module.json"));

    if (!file.open(QIODevice::ReadOnly))
    {
        if (problems)
        {
            *problems = QStringList("There is no module.json in the folder.");
        }

        return false;
    }

    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        if (problems)
        {
            *problems = QStringList(QString("module.json is not valid JSON: %1").arg(error.errorString()));
        }

        return false;
    }

    ModuleManifest m;

    if (!parseManifest(document.object(), &m, problems))
    {
        return false;
    }

    m.directory = QFileInfo(directory).absoluteFilePath();

    // The program has to exist, inside the folder (also after resolving links)
    if (!QFileInfo(m.executablePath()).isFile() || !isInside(m.executablePath(), m.directory))
    {
        if (problems)
        {
            *problems = QStringList(QString("The program \"%1\" does not exist inside the folder of the module.").arg(m.executable));
        }

        return false;
    }

    *manifest = m;
    return true;
}

QList<ModuleManifest> ExternalModules::discover(const QStringList &directories, QStringList *problems)
{
    QList<ModuleManifest> modules;

    for (const QString &directory : directories)
    {
        for (const QFileInfo &entry : QDir(directory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        {
            ModuleManifest manifest;
            QStringList why;

            if (loadManifest(entry.absoluteFilePath(), &manifest, &why))
            {
                modules << manifest;
            }
            else if (problems && QFileInfo(QDir(entry.absoluteFilePath()).filePath("module.json")).exists())
            {
                *problems << QString("%1: %2").arg(entry.fileName(), why.join(" "));
            }
        }
    }

    std::sort(modules.begin(), modules.end(), [](const ModuleManifest &a, const ModuleManifest &b) { return a.name.compare(b.name, Qt::CaseInsensitive) < 0; });
    return modules;
}

QByteArray ExternalModules::runRequest(const QString &inputFolder, const QString &outputFolder, const QJsonObject &parameters)
{
    QJsonObject request;
    request["type"] = "run";
    request["protocol"] = 1;
    request["inputFolder"] = QDir::toNativeSeparators(QFileInfo(inputFolder).absoluteFilePath());
    request["outputFolder"] = QDir::toNativeSeparators(QFileInfo(outputFolder).absoluteFilePath());
    request["parameters"] = parameters;
    return QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";
}

ExternalModules::Message ExternalModules::parseMessage(const QByteArray &line)
{
    Message message;

    if (line.size() > MaxLineLength)
    {
        message.text = "The line is too long.";
        return message;
    }

    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(line, &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        message.text = "The line is not a JSON object.";
        return message;
    }

    QJsonObject o = document.object();
    QString type = o.value("type").toString();

    if (type == "hello")
    {
        if (o.value("protocol").toInt(0) != 1)
        {
            message.text = "The module does not speak protocol 1.";
            return message;
        }

        message.type = Message::Hello;
        message.name = o.value("name").toString();
        message.version = o.value("version").toString();
    }
    else if (type == "progress")
    {
        double value = o.value("value").toDouble(-1.0);

        if (!(value >= 0.0 && value <= 1.0))
        {
            message.text = "The progress is not from 0 to 1.";
            return message;
        }

        message.type = Message::Progress;
        message.progress = value;
        message.text = o.value("message").toString();
    }
    else if (type == "log")
    {
        message.type = Message::Log;
        message.text = o.value("message").toString();
    }
    else if (type == "error")
    {
        message.type = Message::Error;
        message.text = o.value("message").toString("The module reported an error.");
    }
    else if (type == "result")
    {
        message.type = Message::Result;

        for (const QJsonValue &v : o.value("outputs").toArray())
        {
            QJsonObject out = v.toObject();
            Message::Output output;
            output.path = out.value("path").toString();
            output.kind = out.value("kind").toString("file");
            output.description = out.value("description").toString();

            if (output.path.isEmpty() || (output.kind != "file" && output.kind != "folder"))
            {
                message.type = Message::Invalid;
                message.text = "An output needs a path and a kind of \"file\" or \"folder\".";
                return message;
            }

            message.outputs << output;
        }

        message.text = o.value("message").toString();
    }
    else
    {
        message.text = QString("The type \"%1\" is not known.").arg(type);
    }

    return message;
}

ModuleRunner::Result ModuleRunner::run(const ModuleManifest &manifest, const QString &inputFolder, const QString &outputFolder, const QJsonObject &parameters,
                                       const std::function<bool(double, const QString&)> &progress)
{
    Result result;

    if (!QFileInfo(inputFolder).isDir())
    {
        result.message = "The folder of the images does not exist.";
        return result;
    }

    if (outputFolder.trimmed().isEmpty() || ExternalModules::isInside(outputFolder, inputFolder) || ExternalModules::isInside(inputFolder, outputFolder))
    {
        result.message = "The output folder is needed, and it cannot contain the input folder or be inside it.";
        return result;
    }

    if (!QDir().mkpath(outputFolder))
    {
        result.message = "The output folder could not be created.";
        return result;
    }

    QProcess process;
    process.setWorkingDirectory(manifest.directory);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert("STARVIEWER_MODULE", "1");
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(manifest.executablePath(), manifest.arguments);

    if (!process.waitForStarted(10000))
    {
        result.message = QString("The module could not be started: %1").arg(process.errorString());
        return result;
    }

    process.write(ExternalModules::runRequest(inputFolder, outputFolder, parameters));

    QElapsedTimer timer;
    timer.start();
    QByteArray buffer;
    bool hello = false;
    bool finished = false;
    bool cancelled = false;
    bool cancelSent = false;
    int messages = 0;
    QString failure;

    auto handleLine = [&](const QByteArray &line) {
        if (line.trimmed().isEmpty())
        {
            return;
        }

        if (++messages > MaxMessages)
        {
            failure = "The module sent too many messages.";
            return;
        }

        ExternalModules::Message m = ExternalModules::parseMessage(line);

        switch (m.type)
        {
            case ExternalModules::Message::Hello:
                hello = true;
                break;

            case ExternalModules::Message::Progress:
                result.progressMessages++;

                if (progress && !progress(m.progress, m.text) && !cancelled)
                {
                    cancelled = true;
                }

                break;

            case ExternalModules::Message::Log:
                result.log << m.text;
                break;

            case ExternalModules::Message::Error:
                failure = m.text;
                finished = true;
                break;

            case ExternalModules::Message::Result:
                if (!hello)
                {
                    failure = "The module did not say hello first.";
                    break;
                }

                if (m.outputs.size() > MaxOutputs)
                {
                    failure = "The module gave too many outputs.";
                    break;
                }

                for (const ExternalModules::Message::Output &output : m.outputs)
                {
                    QString absolute = QDir::isAbsolutePath(output.path) ? output.path : QDir(outputFolder).filePath(output.path);

                    if (!ExternalModules::isInside(absolute, outputFolder))
                    {
                        failure = QString("The output \"%1\" is not inside the output folder: it is refused.").arg(output.path);
                        break;
                    }

                    ExternalModules::Message::Output checked = output;
                    checked.path = QDir::cleanPath(absolute);
                    result.outputs << checked;
                }

                result.message = m.text;
                finished = true;
                break;

            case ExternalModules::Message::Invalid:
                failure = QString("The module wrote something that is not in the protocol: %1").arg(m.text);
                break;
        }
    };

    while (failure.isEmpty() && (process.state() != QProcess::NotRunning || process.bytesAvailable() > 0))
    {
        if (timer.elapsed() > static_cast<qint64>(manifest.timeoutSeconds) * 1000)
        {
            failure = QString("The module took more than %1 seconds and was stopped.").arg(manifest.timeoutSeconds);
            break;
        }

        if (cancelled && !cancelSent)
        {
            process.write("{\"type\":\"cancel\"}\n");
            cancelSent = true;
            timer.restart();
            // A module that does not stop in 10 seconds is killed
            QElapsedTimer grace;
            grace.start();

            while (process.state() != QProcess::NotRunning && grace.elapsed() < 10000)
            {
                process.waitForFinished(200);
            }

            killTree(process);
            failure = "Cancelled.";
            break;
        }

        process.waitForReadyRead(100);
        buffer += process.readAllStandardOutput();

        int index;

        while ((index = buffer.indexOf('\n')) >= 0 && failure.isEmpty())
        {
            handleLine(buffer.left(index));
            buffer.remove(0, index + 1);
        }

        if (buffer.size() > MaxLineLength)
        {
            failure = "The module wrote a line that is too long.";
        }

        if (finished && failure.isEmpty())
        {
            break;
        }
    }

    if (failure.isEmpty() && !finished && !buffer.trimmed().isEmpty())
    {
        handleLine(buffer);
    }

    if (process.state() != QProcess::NotRunning)
    {
        process.closeWriteChannel();

        if (!process.waitForFinished(3000))
        {
            killTree(process);
        }
    }

    if (!failure.isEmpty() || !finished)
    {
        result.success = false;
        result.outputs.clear();
        QString errorOutput = QString::fromUtf8(process.readAllStandardError()).trimmed().right(500);
        result.message = !failure.isEmpty() ? failure : QString("The module ended without a result (exit code %1).%2").arg(process.exitCode()).arg(errorOutput.isEmpty() ? QString() : " " + errorOutput);
        return result;
    }

    result.success = true;
    return result;
}

} // End namespace udg
