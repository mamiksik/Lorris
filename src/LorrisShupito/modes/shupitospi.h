/**********************************************
**    This file is part of Lorris
**    http://tasssadar.github.com/Lorris/
**
**    See README and COPYING
***********************************************/

#ifndef SHUPITOSPI_H
#define SHUPITOSPI_H

#include "shupitomode.h"

class ShupitoSPI : public ShupitoMode
{
    Q_OBJECT
public:
    ShupitoSPI(Shupito *shupito);

protected:
    ShupitoDesc::config *getModeCfg();
    void editIdArgs(QString& id, quint8& id_lenght);
};

#endif // SHUPITOSPI_H