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

#ifndef UDGROUTINGRULES_H
#define UDGROUTINGRULES_H

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace udg {

/**
    Rules that say where DICOM images are sent depending on what they are: a rule has conditions on attributes of the image (the modality, the description of the study or the series, the body part,
    the institution, ...) and a destination (a PACS, by name). The rules and the destinations are written as JSON, which is checked when it is read. It holds no pointers and sends nothing: see RoutingJob.
 */
class RoutingRules {
public:
    enum class Operator {
        Equals,
        NotEquals,
        Contains,
        NotContains,
        StartsWith,
        EndsWith,
        Matches
    };

    struct Condition {
        QString attribute;
        Operator op = Operator::Equals;
        QString value;
        bool caseSensitive = false;
    };

    struct Rule {
        QString name;
        bool enabled = true;
        /// True: all the conditions have to be true; false: one is enough
        bool matchAll = true;
        QList<Condition> conditions;
        QString destination;
    };

    struct Destination {
        QString name;
        QString host;
        int port = 104;
        QString calledAet;
        QString callingAet = "STARVIEWER";
    };

    QList<Rule> rules;
    QList<Destination> destinations;

    /// The attributes that conditions can use (their DICOM names)
    static QStringList attributeNames();
    static QString operatorName(Operator op);

    /// True if the rule is enabled, has at least one condition and the conditions are true for the values (an attribute that the image does not have is an empty value)
    static bool matches(const Rule &rule, const QMap<QString, QString> &attributes);

    /// The destinations of the enabled rules that match, once each, in the order of the rules
    QList<Destination> destinationsFor(const QMap<QString, QString> &attributes, QStringList *matchedRules = nullptr) const;

    QJsonObject toJson() const;

    /// Reads and checks rules: names, attributes, operators, regular expressions, ports and the destinations that the rules use. On failure the rules are not changed and the reasons are given.
    bool fromJson(const QJsonObject &object, QStringList *problems = nullptr);

    /// The problems of the rules as they are now (the same checks as fromJson)
    QStringList validate() const;

    const Destination* findDestination(const QString &name) const;
};

} // End namespace udg

#endif
