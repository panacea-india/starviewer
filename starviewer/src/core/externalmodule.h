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

#ifndef UDGEXTERNALMODULE_H
#define UDGEXTERNALMODULE_H

#include <functional>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace udg {

/**
    The boundary for modules that run as a separate program (see doc/extension-boundary.md): a module is a folder with a manifest (module.json) and a program. The program is started by Starviewer,
    told where the images are and where to write, and talks by lines of JSON on its standard input and output (protocol 1). It is not loaded in the process of the viewer, so that what it does
    (an AI model with its own libraries, a script) cannot crash the viewer, and it is only given the folders the user chose. This file has the manifest and the messages; ModuleRunner has the process.
 */
struct ModuleManifest {
    QString name;
    QString version;
    QString description;
    /// The program, as a path inside the folder of the module (no .. and no absolute path), and the arguments given to it
    QString executable;
    QStringList arguments;
    /// A module that is marked as research is shown as such and is not meant for clinical use (AI functions can raise the class of a product)
    bool researchOnly = true;
    /// What the module says about the network: "none" or a text with what it connects to
    QString network = "none";
    int timeoutSeconds = 600;
    QString directory;

    /// The absolute path of the program
    QString executablePath() const;
};

class ExternalModules {
public:
    /// Reads and checks module.json in the folder. False and the reasons if it is not valid.
    static bool loadManifest(const QString &directory, ModuleManifest *manifest, QStringList *problems = nullptr);
    static bool parseManifest(const QJsonObject &object, ModuleManifest *manifest, QStringList *problems = nullptr);

    /// The valid modules found in the folders (each module is a subfolder with a module.json), ordered by name; the invalid ones are listed in the problems
    static QList<ModuleManifest> discover(const QStringList &directories, QStringList *problems = nullptr);

    /// The first line that the module gets: the request to run
    static QByteArray runRequest(const QString &inputFolder, const QString &outputFolder, const QJsonObject &parameters);

    struct Message {
        enum Type { Hello, Progress, Log, Result, Error, Invalid } type = Invalid;
        QString text;
        double progress = 0.0;
        struct Output {
            QString path;
            QString kind;
            QString description;
        };
        QList<Output> outputs;
        QString name;
        QString version;
    };

    /// Reads one line of what the module writes. Anything that is not a message of the protocol (a line of text, JSON with another type, a progress out of 0 to 1) is Invalid with the reason in text.
    static Message parseMessage(const QByteArray &line);

    /// True if the path (a file or a folder that may not exist yet) is inside the folder, after resolving . and .. and links of what exists
    static bool isInside(const QString &path, const QString &folder);
};

class ModuleRunner {
public:
    struct Result {
        bool success = false;
        QString message;
        QList<ExternalModules::Message::Output> outputs;
        QStringList log;
        int progressMessages = 0;
    };

    /// Runs the module on the files of the input folder, which it reads, and the output folder, where it writes (created if needed). The progress function gets the value from 0 to 1 and the text,
    /// and returns false to cancel (the module is asked to stop, and killed if it does not). The module is killed after the time of its manifest. The outputs that are not inside the output
    /// folder are refused. The module has to say hello first, with protocol 1, and to end with a result or an error.
    static Result run(const ModuleManifest &manifest, const QString &inputFolder, const QString &outputFolder, const QJsonObject &parameters = QJsonObject(),
                      const std::function<bool(double, const QString&)> &progress = nullptr);
};

} // End namespace udg

#endif
