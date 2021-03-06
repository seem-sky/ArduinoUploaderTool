#ifndef UPLOADER_MAC_H
#define UPLOADER_MAC_H

#include "UploadBase.h"

/*!
 * \brief The Uploader_Mac class
 */
class Uploader_Mac : public UploadBase
{
    Q_OBJECT
public:
    explicit Uploader_Mac(const QString &codePath, const QString &serial, int boardIndex, QObject *parent = 0);
    virtual ~Uploader_Mac();

signals:

public slots:

protected:

};

#endif // UPLOADER_MAC_H
