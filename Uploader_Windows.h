#ifndef UPLOADER_WINDOWS_H
#define UPLOADER_WINDOWS_H

#include "UploadBase.h"
/*!
 * \brief The Uploader_Windows class
 */
class Uploader_Windows : public UploadBase
{
    Q_OBJECT
public:
    explicit Uploader_Windows(QObject *parent = 0);

    //interface
    virtual void setup(); //! 准备
    virtual void compile(); //! 编译
    virtual void writePro();//! 烧写
    virtual void clear(); //! 清理

signals:

public slots:

};

#endif // UPLOADER_WINDOWS_H