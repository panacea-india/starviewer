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

#include "routingrules.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

namespace udg {

namespace {

const char *OperatorNames[] = {"equals", "notEquals", "contains", "notContains", "startsWith", "endsWith", "matches"};

bool operatorFromName(const QString &name, RoutingRules::Operator *op)
{
    for (int i = 0; i < 7; i++)
    {
        if (name == OperatorNames[i])
        {
            *op = static_cast<RoutingRules::Operator>(i);
            return true;
        }
    }

    return false;
}

}

QStringList RoutingRules::attributeNames()
{
    return QStringList() << "Modality" << "StudyDescription" << "SeriesDescription" << "BodyPartExamined" << "InstitutionName" << "StationName" << "Manufacturer" << "ProtocolName"
                         << "PatientSex" << "ImageType" << "SOPClassUID" << "StudyDate" << "ReferringPhysicianName";
}

QString RoutingRules::operatorName(Operator op)
{
    return OperatorNames[static_cast<int>(op)];
}

bool RoutingRules::matches(const Rule &rule, const QMap<QString, QString> &attributes)
{
    if (!rule.enabled || rule.conditions.isEmpty())
    {
        return false;
    }

    for (const Condition &c : rule.conditions)
    {
        QString actual = attributes.value(c.attribute);
        Qt::CaseSensitivity cs = c.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        bool result = false;

        switch (c.op)
        {
            case Operator::Equals:
                result = actual.compare(c.value, cs) == 0;
                break;

            case Operator::NotEquals:
                result = actual.compare(c.value, cs) != 0;
                break;

            case Operator::Contains:
                result = actual.contains(c.value, cs);
                break;

            case Operator::NotContains:
                result = !actual.contains(c.value, cs);
                break;

            case Operator::StartsWith:
                result = actual.startsWith(c.value, cs);
                break;

            case Operator::EndsWith:
                result = actual.endsWith(c.value, cs);
                break;

            case Operator::Matches:
            {
                QRegularExpression regex(c.value, c.caseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
                result = regex.isValid() && regex.match(actual).hasMatch();
                break;
            }
        }

        if (rule.matchAll && !result)
        {
            return false;
        }

        if (!rule.matchAll && result)
        {
            return true;
        }
    }

    return rule.matchAll;
}

const RoutingRules::Destination* RoutingRules::findDestination(const QString &name) const
{
    for (const Destination &d : destinations)
    {
        if (d.name == name)
        {
            return &d;
        }
    }

    return nullptr;
}

QList<RoutingRules::Destination> RoutingRules::destinationsFor(const QMap<QString, QString> &attributes, QStringList *matchedRules) const
{
    QList<Destination> result;
    QSet<QString> seen;

    for (const Rule &rule : rules)
    {
        if (!matches(rule, attributes))
        {
            continue;
        }

        const Destination *destination = findDestination(rule.destination);

        if (!destination)
        {
            continue;
        }

        if (matchedRules)
        {
            *matchedRules << rule.name;
        }

        if (!seen.contains(destination->name))
        {
            seen.insert(destination->name);
            result << *destination;
        }
    }

    return result;
}

QStringList RoutingRules::validate() const
{
    QStringList problems;
    QSet<QString> destinationNames;

    for (const Destination &d : destinations)
    {
        if (d.name.trimmed().isEmpty())
        {
            problems << "A destination has no name.";
        }
        else if (destinationNames.contains(d.name))
        {
            problems << QString("The destination \"%1\" is twice.").arg(d.name);
        }

        destinationNames.insert(d.name);

        if (d.host.trimmed().isEmpty())
        {
            problems << QString("The destination \"%1\" has no host.").arg(d.name);
        }

        if (d.port < 1 || d.port > 65535)
        {
            problems << QString("The destination \"%1\" has a port that is not from 1 to 65535.").arg(d.name);
        }

        if (d.calledAet.trimmed().isEmpty() || d.calledAet.length() > 16 || d.callingAet.trimmed().isEmpty() || d.callingAet.length() > 16)
        {
            problems << QString("The AE titles of the destination \"%1\" must have from 1 to 16 characters.").arg(d.name);
        }
    }

    QSet<QString> ruleNames;
    QStringList attributes = attributeNames();

    for (const Rule &r : rules)
    {
        QString label = r.name.trimmed().isEmpty() ? QString("A rule") : QString("The rule \"%1\"").arg(r.name);

        if (r.name.trimmed().isEmpty())
        {
            problems << "A rule has no name.";
        }
        else if (ruleNames.contains(r.name))
        {
            problems << QString("The rule \"%1\" is twice.").arg(r.name);
        }

        ruleNames.insert(r.name);

        if (r.conditions.isEmpty())
        {
            problems << QString("%1 has no conditions.").arg(label);
        }

        if (!destinationNames.contains(r.destination))
        {
            problems << QString("%1 sends to \"%2\", which is not a destination.").arg(label, r.destination);
        }

        for (const Condition &c : r.conditions)
        {
            if (!attributes.contains(c.attribute))
            {
                problems << QString("%1 uses the attribute \"%2\", which is not known (%3).").arg(label, c.attribute, attributes.join(", "));
            }

            if (c.op == Operator::Matches && !QRegularExpression(c.value).isValid())
            {
                problems << QString("%1 has a regular expression that is not valid: %2").arg(label, c.value);
            }
        }
    }

    return problems;
}

QJsonObject RoutingRules::toJson() const
{
    QJsonArray destinationArray;

    for (const Destination &d : destinations)
    {
        QJsonObject o;
        o["name"] = d.name;
        o["host"] = d.host;
        o["port"] = d.port;
        o["calledAet"] = d.calledAet;
        o["callingAet"] = d.callingAet;
        destinationArray.append(o);
    }

    QJsonArray ruleArray;

    for (const Rule &r : rules)
    {
        QJsonObject o;
        o["name"] = r.name;
        o["enabled"] = r.enabled;
        o["match"] = r.matchAll ? "all" : "any";
        o["destination"] = r.destination;
        QJsonArray conditionArray;

        for (const Condition &c : r.conditions)
        {
            QJsonObject co;
            co["attribute"] = c.attribute;
            co["operator"] = operatorName(c.op);
            co["value"] = c.value;
            co["caseSensitive"] = c.caseSensitive;
            conditionArray.append(co);
        }

        o["conditions"] = conditionArray;
        ruleArray.append(o);
    }

    QJsonObject object;
    object["destinations"] = destinationArray;
    object["rules"] = ruleArray;
    return object;
}

bool RoutingRules::fromJson(const QJsonObject &object, QStringList *problems)
{
    RoutingRules parsed;
    QStringList found;

    if (!object.value("destinations").isArray() || !object.value("rules").isArray())
    {
        found << "The file needs a list of \"destinations\" and a list of \"rules\".";
    }

    for (const QJsonValue &v : object.value("destinations").toArray())
    {
        QJsonObject o = v.toObject();
        Destination d;
        d.name = o.value("name").toString();
        d.host = o.value("host").toString();
        d.port = o.value("port").toInt(0);
        d.calledAet = o.value("calledAet").toString();
        d.callingAet = o.contains("callingAet") ? o.value("callingAet").toString() : QString("STARVIEWER");
        parsed.destinations << d;
    }

    for (const QJsonValue &v : object.value("rules").toArray())
    {
        QJsonObject o = v.toObject();
        Rule r;
        r.name = o.value("name").toString();
        r.enabled = o.contains("enabled") ? o.value("enabled").toBool(true) : true;
        QString match = o.value("match").toString("all");

        if (match != "all" && match != "any")
        {
            found << QString("The rule \"%1\" has \"match\" that is not \"all\" or \"any\".").arg(r.name);
        }

        r.matchAll = match != "any";
        r.destination = o.value("destination").toString();

        for (const QJsonValue &cv : o.value("conditions").toArray())
        {
            QJsonObject co = cv.toObject();
            Condition c;
            c.attribute = co.value("attribute").toString();
            c.value = co.value("value").toString();
            c.caseSensitive = co.value("caseSensitive").toBool(false);

            if (!operatorFromName(co.value("operator").toString("equals"), &c.op))
            {
                found << QString("The rule \"%1\" has the operator \"%2\", which is not known (%3).").arg(r.name, co.value("operator").toString(), QStringList(OperatorNames, OperatorNames + 7).join(", "));
            }

            r.conditions << c;
        }

        parsed.rules << r;
    }

    found << parsed.validate();

    if (!found.isEmpty())
    {
        if (problems)
        {
            *problems = found;
        }

        return false;
    }

    *this = parsed;
    return true;
}

} // End namespace udg
