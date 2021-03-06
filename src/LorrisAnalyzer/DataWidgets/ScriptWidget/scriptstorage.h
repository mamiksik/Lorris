/**********************************************
**    This file is part of Lorris
**    http://tasssadar.github.com/Lorris/
**
**    See README and COPYING
***********************************************/

#ifndef SCRIPTSTORAGE_H
#define SCRIPTSTORAGE_H

#include <QObject>
#include <QHash>
#include <QScriptValue>
#include <QVariantList>

class DataFileParser;

struct ScriptData
{
    ScriptData()
    {
        len = 0;
        data = NULL;
    }

    quint32 len;
    char *data;
};

/**
 * \brief This storage can hold values for script. It persists through script destruction and is saved to data file
 */
class ScriptStorage : public QObject
{
    Q_OBJECT
public:
    typedef QHash<QString, ScriptData*> DataHash;

    explicit ScriptStorage(QObject *parent = 0);
    ~ScriptStorage();

    void saveToFile(DataFileParser *file);
    void loadFromFile(DataFileParser *file);

public slots:
    void clear();
    bool exists(const QString& key);

    void setBool                 (const QString& key, bool val);
    void setUInt32               (const QString& key, quint32 val);
    void setInt32                (const QString& key, qint32 val);
    void setFloat                (const QString& key, float val);
    void setString               (const QString& key, const QString& val);
    void setFloatArray           (const QString& key, const QVariantList& val);
    void setInt32Array           (const QString& key, const QVariantList& val);
    void setUInt32Array          (const QString& key, const QVariantList& val);

    bool         getBool         (const QString& key, bool def = false);
    quint32      getUInt32       (const QString& key, quint32 def = 0);
    qint32       getInt32        (const QString& key, qint32 def = 0);
    float        getFloat        (const QString& key, float def = 0.f);
    QString      getString       (const QString& key, QString def = "");
    QVariantList getFloatArray   (const QString& key, QVariantList def = QVariantList());
    QVariantList getInt32Array   (const QString& key, QVariantList def = QVariantList());
    QVariantList getUInt32Array  (const QString& key, QVariantList def = QVariantList());

private:
    ScriptData *findKey(const QString& key);
    ScriptData *getKeyForSet(const QString& key);

    template <typename T> void setBaseType(const QString &key, T val);
    template <typename T> T getBaseType(const QString &key, T def);

    template <typename T> void setArrayType(const QString& key, QVariantList val);
    template <typename T> QVariantList getArrayType(const QString& key, QVariantList def);

    DataHash m_data;
};

template <typename T>
void ScriptStorage::setBaseType(const QString &key, T val)
{
    ScriptData *sc_data = getKeyForSet(key);

    sc_data->len = sizeof(T);
    sc_data->data = new char[sizeof(T)];

    char *p = (char*)(&val);
    std::copy(p, p+sizeof(T), sc_data->data);
}

template <typename T>
T ScriptStorage::getBaseType(const QString &key, T def)
{
    ScriptData *sc_data = findKey(key);

    T val = def;

    if(!sc_data)
        return val;

    if(sizeof(T) != sc_data->len)
        return val;

    char *p = (char*)(&val);
    std::copy(sc_data->data, sc_data->data + sc_data->len, p);

    return val;
}

template <typename T>
void ScriptStorage::setArrayType(const QString& key, QVariantList val)
{
    ScriptData *sc_data = getKeyForSet(key);

    sc_data->len = val.size()*sizeof(T);
    sc_data->data = new char[sc_data->len];

    quint32 pos = 0;
    for(QVariantList::Iterator itr = val.begin(); itr != val.end(); ++itr)
    {
        T val = (*itr).value<T>();
        char *p = (char*)(&val);

        std::copy(p, p+sizeof(T), sc_data->data+pos);
        pos += sizeof(T);
    }
}

template <typename T>
QVariantList ScriptStorage::getArrayType(const QString& key, QVariantList def)
{
    ScriptData *sc_data = findKey(key);
    if(!sc_data)
        return def;

    if(sc_data->len % sizeof(T) != 0 || sc_data->len < sizeof(T))
        return def;

    QVariantList ret;
    for(quint32 i = 0; i < sc_data->len; i += sizeof(T))
    {
        QVariant item(*( (T*)(sc_data->data + i) ));
        ret.push_back(item);
    }
    return ret;
}

#endif // SCRIPTSTORAGE_H
