#include "UploadBase.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QRegExp>
#include <QProcess>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTcpSocket>

#include "Sleep.h"
#include "Qextserialport/qextserialport.h"
#include "Qextserialport/qextserialenumerator.h"
#include "NetWork.h"

/**
 * @brief 构造函数
 * @param [in] codePath 编译目录
 * @param [in] serial 串口号
 * @param [in] board 开发版型号
 * @param [in] parent
 */
UploadBase::UploadBase(const QString &codePath, const QString &serial, int boardIndex, QObject *parent)
    : QObject(parent)
    , pExternalProcess_(NULL)
    , serialPort_(serial)
    , boardIndex_(boardIndex)
    , compilerPath_("")
    , codePath_(codePath)
    , cmd_("")
    , hexPath_("")
{
	pNetWork = new NetWork(this);
	pNetWork->pTcpSocket_->waitForConnected();

    scanAllLibraryHeaderFile("./Arduino/libraries");
    scanAllheaderFile("./Arduino/libraries");

    pExternalProcess_ = new QProcess(this);
    connect(pExternalProcess_, SIGNAL(readyReadStandardError()), this, SLOT(slotreadyReadStandardError()));
    connect(pExternalProcess_, SIGNAL(readyReadStandardOutput()), this, SLOT(slotReadyReadStandardOutput()));

    //init board information
    {
        map_boardIndex_infor_[0] = Board("Arduino Uno", "atmega328p", "standard", "115200","Uno.a", "16000000L", "arduino", 32256);
        map_boardIndex_infor_[1] = Board("Arduino Duemilanove (328)", "atmega328p", "standard", "57600","Duemilanove328.a", "16000000L", "arduino", 30720);
        map_boardIndex_infor_[2] = Board("Arduino Duemilanove (168)", "atmega168", "standard", "19200","Duemilanove168.a", "16000000L", "arduino", 14336);
        map_boardIndex_infor_[3] = Board("Arduino Nano (328)", "atmega328p", "eightanaloginputs", "57600","Nano328.a", "16000000L", "arduino", 30720);
        map_boardIndex_infor_[4] = Board("Arduino Nano (168)", "atmega168", "eightanaloginputs", "19200","Nano168.a", "16000000L", "arduino", 14336);
        map_boardIndex_infor_[5] = Board("Arduino Mega 2560/ADK", "atmega2560", "mega", "115200", "Mega2560.a", "16000000L", "wiring", 258048);
        map_boardIndex_infor_[6] = Board("Arduino Mega 1280", "atmega1280", "mega", "57600", "Mega1280.a", "16000000L", "arduino", 126976);
        map_boardIndex_infor_[7] = Board("Arduino Leonardo", "atmega32u4", "leonardo", "57600","Leonardo.a", "16000000L", "avr109", 28672);
        map_boardIndex_infor_[8] = Board("Arduino Esplora", "atmega32u4", "leonardo", "57600","Esplora.a", "16000000L", "avr109", 28672);
        map_boardIndex_infor_[9] = Board("Arduino Micro", "atmega32u4", "micro", "57600","Micro.a", "16000000L", "avr109", 28672);
        map_boardIndex_infor_[10] = Board("Arduino Mini (328)", "atmega328p", "eightanaloginputs", "115200","Mini328.a", "16000000L", "arduino", 28672);
        map_boardIndex_infor_[11] = Board("Arduino Mini (168)", "atmega168", "eightanaloginputs", "19200","Mini168.a", "16000000L", "arduino", 14336);
        //Ethernet
        map_boardIndex_infor_[12] = Board("Arduino Ethernet", "atmega328p", "standard", "115200", "Ethernet.a", "16000000L", "arduino", 32256);
        //Fio
        map_boardIndex_infor_[13] = Board("Arduino Fio", "atmega328p", "eightanaloginputs", "57600","Fio.a", "8000000L", "arduino", 30720);
        //BT
        map_boardIndex_infor_[14] = Board("Arduino BT (328)", "atmega328p", "eightanaloginputs", "19200","BT328.a", "16000000L", "arduino", 28672);
        map_boardIndex_infor_[15] = Board("Arduino BT (168)", "atmega168", "eightanaloginputs", "19200","BT168.a", "16000000L", "arduino", 14336);
        //LilyPad
        map_boardIndex_infor_[16] = Board("LilyPad Arduino USB", "atmega32u4", "leonardo", "57600","LilyPadUsb.a", "8000000L", "avr109", 28672);
        map_boardIndex_infor_[17] = Board("LilyPad Arduino (328)", "atmega328p", "standard", "57600","LilyPad328.a", "8000000L", "arduino", 30720);
        map_boardIndex_infor_[18] = Board("LilyPad Arduino (168)", "atmega168", "standard", "19200","LilyPad168.a", "8000000L", "arduino", 14336);
        //ProMini
        map_boardIndex_infor_[19] = Board("Arduino Pro Mini (328) 5V", "atmega328p", "standard", "57600", "ProMini328_5.a", "16000000L", "arduino", 30720);
        map_boardIndex_infor_[20] = Board("Arduino Pro Mini (168) 5V", "atmega168", "standard", "19200", "ProMini168_5.a", "16000000L", "arduino", 14336);
        map_boardIndex_infor_[21] = Board("Arduino Pro Mini (328) 3.3V", "atmega328p", "standard", "57600", "ProMini328_3.a", "8000000L", "arduino", 30720);
        map_boardIndex_infor_[22] = Board("Arduino Pro Mini (168) 3.3V", "atmega168", "standard", "19200", "ProMini168_3.a", "8000000L", "arduino", 14336);
        //NG
        map_boardIndex_infor_[23] = Board("Arduino NG (168)", "atmega168", "standard", "19200","NG168.a", "16000000L", "arduino", 14336);
        map_boardIndex_infor_[24] = Board("Arduino NG", "atmega8", "standard", "19200","NG8.a", "16000000L", "arduino", 7168);
    }
}

UploadBase::~UploadBase()
{
}

void UploadBase::start()
{
    prepare();//prepare to compile
    compile();//compileLibrary, linkerCommand
    writePro();//getUploadCommand, and call QProcess to execute
}

/**
 * @brief 遍历获取某个父目录下的所有子目录的路径
 * @param [in] parentDirPath 父目录路径
 * @return 所有子目录的路径
 */
QSet<QString> UploadBase::getAllChildDirPath(const QString &parentDirPath)
{
    QDir dir(parentDirPath);
    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

    QSet<QString> tmpDirs;
    if(dir.entryList().isEmpty())
    {
        QSet<QString>tmp;
        tmp << parentDirPath;
        return tmp;
    }
    else
    {
        foreach (const QString &dirName, dir.entryList())
        {
            QString childDirPath = parentDirPath + "/" + dirName;
            if(childDirPath.contains("examples")
                    || childDirPath.contains("Examples"))
            {
                continue;
            }
            tmpDirs << childDirPath;
            tmpDirs += getAllChildDirPath(childDirPath);
        }
    }

    return tmpDirs;
}

/**
 * @brief 获取调用avr-gcc的编译命令
 * @param [in] sketchPath 要编译的文件路径
 * @param [in] cpuType CPU的类型,参照Arduino官方
 * @param [in] libPaths 需要的Library的路径
 * @param [in] workPath 编译的工作路径
 * @param [in] workingFrequency 开发版的工作频率
 * @return
 */
QString UploadBase::getCompilerCommand(const QString &sketchPath, const QString &cpuType,
                                       const QString &var, const QList<QString> &libPaths, QString workingFrequency, QString workPath)
{
	QFileInfo infor(sketchPath);
	QString suffix = infor.suffix();
	QString cmd;

	QString id = QString("-DUSB_VID=null -DUSB_PID=null");
	{
		if(infor.baseName() == QFileInfo(codePath_).baseName())
		{
			//针对leonardo特殊的处理
			if ("leonardo" == var)
			{
				id = " -DUSB_VID=0x2341 -DUSB_PID=0x8036";
			}
			else if ("Micro" == var)
			{
				id = " -DUSB_VID=0x2341 -DUSB_PID=0x8037";
			}
			else if ("Esplora" == var)
			{
				id = " -DUSB_VID=0x2341 -DUSB_PID=0x803C";
			}
		}
	}
	if("c" == suffix
			|| "C" == suffix)
	{
		cmd = QString("%1/avr-gcc -c -g -Os -Wall -ffunction-sections -fdata-sections -mmcu=%2 -DF_CPU=%3 -MMD %4 -DARDUINO=103 ").arg("./Arduino/hardware/tools/avr/bin").arg(cpuType).arg(workingFrequency).arg(id);
	}
	else if("cpp" == suffix
			|| "CPP" == suffix
			|| "java.exe" == infor.fileName())
	{
		cmd = QString("%1/avr-g++ -c -g -Os -Wall -fno-exceptions -ffunction-sections -fdata-sections -mmcu=%2 -DF_CPU=%3 -MMD %4 -DARDUINO=103 ").arg("./Arduino/hardware/tools/avr/bin").arg(cpuType).arg(workingFrequency).arg(id);
	}
	else
	{
		return QString();
	}

	cmd += "-I./Arduino/hardware/arduino/cores/arduino ";
	cmd += "-I./Arduino/hardware/arduino/variants/standard ";

	//加引用路径
	for(int i = 0; i != libPaths.size(); ++i)
	{
		cmd += QString("-I")+libPaths.at(i) + " ";
	}
	//这里结构测试的时候再仔细考虑一下
	if(sketchPath.contains("/")
			|| sketchPath.contains("\\"))
	{
		cmd += QString("-I") + QFileInfo(sketchPath).path() + " ";
	}
	cmd += QString(sketchPath + " -o "  +workPath + "/" + infor.fileName() + ".o");
#ifdef USE_DEBUG
	QFile file("cmd.txt");
	if(!file.open(QFile::Append))
	{
		qDebug() << "cmd.txt can't open!!";
	}
	file.write(cmd.toUtf8().data());
	file.write("\n");
	file.close();
#endif
	return cmd;
}

/**
 * @brief UploadBase::linkerCommand
 * @param filePath
 * @param cpuType
 * @param staticLibraryPath
 * @param workPath
 * @param workingFrequency
 */
void UploadBase::linkerCommand(const QString &filePath, const QString &cpuType, const QString &staticLibraryPath, QString workPath)
{
    QString elfPath = filePath + ".elf";
    QString eepPath = filePath + ".eep";
    QString hexPath = filePath + ".hex";
    hexPath_ = hexPath;
    QString elf = create_elf_fileCommand(filePath, cpuType, staticLibraryPath, workPath);
    //这部分到整合目录之后就可以统一了
    QString eep = create_eep_fileCommand("./Arduino/hardware/tools/avr/bin/avr-objcopy", elfPath, eepPath);
    QString hex = create_hex_fileCommand("./Arduino/hardware/tools/avr/bin/avr-objcopy",elfPath, hexPath);

    //    qDebug() << "elf: " << elf;
    //    qDebug() << "eep: " << eep;
    //    qDebug() << "hex: " << hex;

    //调用QProcess
    if(pExternalProcess_)
    {
		//这里需要用同步以保证结果正确
//        pExternalProcess_->execute(elf);//生成elf文件
//        pExternalProcess_->execute(eep);//生成eep文件
//        pExternalProcess_->execute(hex);//生成hex文件
		pExternalProcess_->start(elf);//生成elf文件
		pExternalProcess_->waitForFinished();
		pExternalProcess_->start(eep);//生成eep文件
		pExternalProcess_->waitForFinished();
		pExternalProcess_->start(hex);//生成hex文件
		pExternalProcess_->waitForFinished();

        if(QFile::exists(hexPath))
        {
            cout << "Compiliation successful completed!" << endl;
        }
    }
}

/**
 * @brief 获取创建elf文件的命令
 * @param [in] filePath 文件路径
 * @param [in] cpuType CPU类型,参考Arduino官方的类型设置
 * @param [in] staticLibraryPath 要链接的静态库路径, 这个静态库可以通过Arduino官方的IDE编译生成
 * @param [in] workPath 编译文件的临时目录
 * @param [in] workingFrequency 开发版的工作频率, 默认16Mhz目前不做使用
 * @return 可以扔给avr-gcc的命令
 */
QString UploadBase::create_elf_fileCommand(const QString &filePath, const QString &cpuType, const QString &staticLibraryPath, QString workPath)
{
    QString baseName = QFileInfo(filePath).fileName();
    QString cmd = QString("%1 -Os -Wl,--gc-sections -mmcu=%2 -o %3 ").arg("./Arduino/hardware/tools/avr/bin/avr-gcc").arg(cpuType).arg(workPath + "/" + baseName + ".elf");

    QDir dir(workPath);
    dir.setFilter(QDir::Files);
    QStringList tmp = dir.entryList();
    foreach (const QString &fileName, tmp)
    {
        if(fileName.contains(".cpp.o")
                || fileName.contains(".c.o"))
        {
            cmd += workPath+"/" + fileName + " ";
        }
    }

    cmd += staticLibraryPath + " -L" + workPath + " -lm";

    return cmd;
}

/**
 * @brief 获取生成eep文件的命令
 * @param [in] toolPath avr-objcopy的路径
 * @param [in] elfPath elf文件路径(elf文件由create_elf_fileCommand函数生成)
 * @see [in] create_elf_fileCommand
 * @param [in] eepPath 要生成的eep文件路径
 * @return 可以扔给avr-gcc的命令
 */
QString UploadBase::create_eep_fileCommand(const QString &toolPath, const QString &elfPath, const QString &eepPath)
{
    QString cmd = QString("%1 -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 %2 %3").arg(toolPath).arg(elfPath).arg(eepPath);

    return cmd;
}

/**
 * @brief 获取生成hex文件的命令
 * @param [in] toolPath avr-objcopy的路径
 * @param [in] elfPath elf文件路径(elf文件由create_elf_fileCommand函数生成)
 * @see [in] create_elf_fileCommand
 * @param [in] hexPath 要生成的hex文件路径
 * @return 可以扔给avr-gcc的命令
 */
QString UploadBase::create_hex_fileCommand(const QString &toolPath, const QString &elfPath, const QString &hexPath)
{
    QString cmd = QString("%1 -O ihex -R .eeprom %2 %3").arg(toolPath).arg(elfPath).arg(hexPath);

    return cmd;
}

/**
 * @brief 获取上传命令
 * @param [in] avrdudePath avrdude的路径, 上传功能是依靠avrdude的
 * @param [in] configPath avrdudeconfig路径
 * @param [in] cpuType CPU类型
 * @param [in] serialPort 串口号
 * @param [in] baudrate 波特率
 * @param [in] hexPath 要上传的hex文件路径
 * @param [in] protocol arduino(一般arduino类型),  avr109(leonardo), wiring(mega2560 编程器)
 * @return 扔给avrdude的命令
 */
QString UploadBase::getUploadCommand(const QString &avrdudePath, const QString &configPath,
                                     const QString &cpuType, const QString &serialPort, const QString &baudrate,
                                     const QString &hexPath, const QString &protocol)
{
    QString cmd = QString("%1 -C%2 -v -v -v -v -p%3 -c%7 -P%4 -b%5 -D -Uflash:w:%6:i").arg(avrdudePath).arg(configPath).arg(cpuType).arg(serialPort).arg(baudrate).arg(hexPath).arg(protocol);

    return cmd;
}

/**
 * @brief 递归编译指定库目录中的所有*c,*cpp
 * @param [in] libraryDirPath 库目录路径
 */
void UploadBase::compileLibrary(const QString &libraryDirPath, const QString &mcu, const QString &var, const QString &workFrequency)
{
    QDir dir(libraryDirPath);
    QStringList filters;
    filters << "*.cpp" << "*.CPP" << "*.cxx" << "*.cc" << "*.c" << "*.C";
    dir.setNameFilters(filters);
    QStringList fileNames = dir.entryList();
    foreach (const QString &fileName, fileNames)
    {
        QString filePath = libraryDirPath + "/" + fileName;

        QSet<QString> headerFiles = getAllReferenceHeaderFileSet(filePath);
        QSet<QString> libPaths;
        foreach (const QString &headerFile, headerFiles)
        {
            if(map_headerFile_path_.contains(headerFile))
            {
                if(map_headerFile_path_.count(headerFile) > 1)
                {//处理有重复的情况, 有重复的情况下只处理子目录下的
                    QMultiMap<QString, QString>::iterator iter = map_headerFile_path_.find(headerFile);
                    while (iter != map_headerFile_path_.end()
                           && headerFile == iter.key())
                    {
                        QString childFilePath =iter.value();
                        QString parentPath = QFileInfo(filePath).path();
                        if(childFilePath.contains(parentPath))
                        {//子目录路径必定包含父级目录的路径
                            libPaths += parentPath;
                        }
                        ++iter;
                    }
                }
                else
                {
                    QString childFilePath = map_headerFile_path_.value(headerFile);
                    if(!childFilePath.isEmpty())
                    {
                        libPaths += childFilePath;
                    }
                }
            }
            else
            {//
                //qDebug() << "can't find this header file " << headerFile;
            }
        }

        //编译本文件
        QString cmd = getCompilerCommand(filePath, mcu, var, libPaths.toList());
        //qDebug() << cmd;
		//pExternalProcess_->execute(cmd);
		pExternalProcess_->start(cmd);
		pExternalProcess_->waitForFinished();
        alreadyCompile_.clear();
    }

    //递归遍历子目录
    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    QStringList dirNames = dir.entryList();
    if(dirNames.isEmpty())
    {
        return;
    }
    else
    {
        foreach (const QString &dirName, dirNames)
        {
            if(".settings" == dirName
                    || "examples" == dirName
                    || "doc" == dirName
                    || "Examples" == dirName
                    || ".svn" == dirName)
            {
                continue;
            }
            compileLibrary(libraryDirPath + "/" + dirName, mcu, var, workFrequency);
        }
    }
}

/**
 * @brief 递归复制复制目录
 * @param srcPath [in] 源目录
 * @param desPath [in] 目标目录
 * @return 返回执行结果
 * @retval true 执行成功
 * @retval false 执行失败
 */
void UploadBase::copyDirectory(const QString &srcPath, const QString &desPath)
{
    QDir dir(srcPath);
    {
        dir.setFilter(QDir::Files);
        foreach (const QString &fileName, dir.entryList())
        {
            QString srcFilePath = srcPath + "/" + fileName;
            QString desFilePath = srcPath + "/" + fileName;
            if(!copyFile(srcFilePath, desFilePath))
            {
                qDebug() << "copy file error! from " << srcFilePath << "to " << desFilePath;
            }
        }
    }
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList dirNames = dir.entryList();
    if(dirNames.isEmpty())
    {
        return;
    }
    else
    {
        foreach (const QString &dirName, dirNames)
        {
            QString newSrcPath = srcPath + "/" + dirName;
            QString newDesPath = desPath + "/" + dirName;
            copyDirectory(newSrcPath, newDesPath);
        }
    }
}

/**
 * @brief 复制文件,从srcPath到desPath
 * @param srcPath [in] 文件原路径
 * @param desPath [in] 文件目标路径
 * @retval true 执行成功
 * @retval false 执行失败
 * @pre 目的路径要存在(mkpath)
 */
bool UploadBase::copyFile(const QString &srcPath, const QString &desPath)
{
    //读数据
    QFile file(srcPath);
    if(!file.open(QFile::ReadOnly))
    {
        qDebug() << "open src file fail" << srcPath;
        return false;
    }

    QByteArray bytes =  file.readAll();
    file.close();

    ////////////////////////////////
    //在这里要确保path可用
    {
        QDir pathTest("./");
        QString oldPath = pathTest.currentPath();

        pathTest.setPath(desPath);
        if(!pathTest.exists())
        {
            qDebug() << "not exists the path, and it will try to cerate it:" << desPath;
            if(!pathTest.mkpath(desPath))
            {
                qDebug() << "create path fail!!!!";
                return false;
            }
        }
        pathTest.setPath(oldPath);
    }
    ////////////////////////////////

    QFile desFile(desPath);
    if(!desFile.open(QFile::WriteOnly))
    {
        qDebug() << "open des file fail " << desPath;
        return false;
    }

    desFile.write(bytes);
    desFile.waitForBytesWritten(10);
    desFile.close();

	return true;
}

QVariantMap UploadBase::getInfor(const QString &infor)
{
	QRegExp regExp("error: invalid conversion from '.+' to '.+'");

	QVariantMap map;
	//编译错误--类型转换错误
	int pos = infor.indexOf(regExp);
	if(pos >= 0)
	{
		QString tmp = regExp.cap(0).left(regExp.cap(0).indexOf("\n"));
		qDebug() << "regExp.cap(0) :" << tmp;
		qDebug() << "regExp.cap(1) :" << regExp.cap(1);
		qDebug() << "regExp.cap(2) :" << regExp.cap(2);

		map = pNetWork->getGrammaticalError(infor);
	}
	else if(infor.contains("no new port found"))
	{
		map = pNetWork->getSerialPortError(infor);
	}
	else if(infor.contains("avrdude: ser_open(): can't open device"))
	{//上传错误--端口号错误
		//对外通知端口号错误
		map = pNetWork->getSerialPortError("serialport error ,and can't open device");
	}
	else if(infor.contains("not in sync"))
	{//如果猜得没错就是它了
		 //对外通知板子类型错误
		map = pNetWork->getBoradError("serialport number error");
	}
	else if(infor.contains("bytes of flash verified")
			&& infor.contains("100%"))
	{//编译上传成功
		map = pNetWork->getUploaderSuccess("uploader success!");
	}
	else if(infor.contains("Compiliation successful completed"))
	{
		map = pNetWork->getCompileSuccess("compiliation successful completed");
	}
	else if(infor.contains("error opening port"))
	{
		map = pNetWork->getSerialPortError("error opening port");
	}
	else
	{
		map = pNetWork->getOtherInfor("unkown information");
	}

	return map;
}

void UploadBase::prepare()
{
    QSet<QString> headerFiles = getReferenceHeaderFilesFromSingleFile(codePath_);
    QSet<QString> tmpLibDirPath;
    foreach (const QString header, headerFiles)
    {
        if(map_libName_infor_.contains(header))
        {
            tmpLibDirPath << map_libName_infor_.value(header).libPath;
        }
    }

    libraryPaths_ = tmpLibDirPath;
}

void UploadBase::compile()
{
    //这里的CPU类型是需要通过一个数据结构获取的
    if(map_boardIndex_infor_.contains(boardIndex_))
    {
        QString mcu = map_boardIndex_infor_[boardIndex_].mcu;
        QString var = map_boardIndex_infor_[boardIndex_].variant;
        QString frequency = map_boardIndex_infor_[boardIndex_].workingFrequency;
        QString cmd = getCompilerCommand(codePath_, mcu, var, libraryPaths_.toList(), frequency);
        //qDebug() << cmd;
        //pExternalProcess_->execute(cmd);
        pExternalProcess_->start(cmd);
        pExternalProcess_->waitForFinished();
        foreach (const QString &dirPath, libraryPaths_)
        {
            compileLibrary(dirPath, mcu, var, frequency);
        }

        QString staticLibrary = map_boardIndex_infor_[boardIndex_].staticLibrary;
        QString staticLibPath = QString("./Arduino/staticLibrary/%1").arg(staticLibrary);
        linkerCommand(codePath_, mcu, staticLibPath);
    }
    else
    {
        qDebug() << "can't find index: "<< boardIndex_;
        return;
    }
}

void UploadBase::writePro()
{
    if(map_boardIndex_infor_.contains(boardIndex_))
    {
        QString mcu = map_boardIndex_infor_[boardIndex_].mcu;
        QString baudrate = map_boardIndex_infor_[boardIndex_].baudrate;
        QString serialPort = serialPort_;
        QString protocol = map_boardIndex_infor_[boardIndex_].protocol;

        if("leonardo" == map_boardIndex_infor_[boardIndex_].variant)
        {
            QSet<QString> portsOld;
            foreach (QextPortInfo portInfor, QextSerialEnumerator::getPorts())
            {
                portsOld << portInfor.portName;
            }

            PortSettings setting;
            setting.BaudRate = BAUD1200;
            //setting.FlowControl = FLOW_OFF;
            setting.DataBits = DATA_8;
            setting.StopBits = STOP_1;
            setting.Parity = PAR_NONE;
#ifdef Q_OS_WIN32
            QextSerialPort *pScanSerialPort = new QextSerialPort(serialPort_, setting);
            //QextSerialPort *pScanSerialPort = new QextSerialPort(serialPort_, setting);
#else
			QString tmpPort = QString("/dev/")+serialPort_;
			QString cmd = "./reset ";
			cmd += tmpPort;
			qDebug() << cmd;
			//QProcess::execute(cmd);
			QProcess pro;
			pro.start(cmd);
			pro.waitForFinished();
			pro.close();
#endif

#ifndef Q_OS_MAC
			if(!pScanSerialPort->open(QIODevice::ReadWrite))
			{
				qDebug() << "serial prot open fail!";
				qDebug() << pScanSerialPort->errorString();
			}
			pScanSerialPort->close();
#endif

#ifdef Q_OS_WIN32
            QSet<QString> portsNew;
            do
            {
                QList<QextPortInfo> tmp = QextSerialEnumerator::getPorts();
                foreach (QextPortInfo portInfor, tmp)
                {
                    portsNew << portInfor.portName;
                }
            }while (portsNew.size() <= portsOld.size());

            QSet<QString> result = portsNew - portsOld;
            if(!result.isEmpty())
            {
                serialPort = *(result.begin());
            }
#elif defined(Q_OS_MAC)

            QSet<QString> portsNew;
            QList<QextPortInfo> tmp;

            QSet<QString> result;
            int elapsed = 0;
            QString foundPort;
			while (elapsed < 2000)
            {
                tmp = QextSerialEnumerator::getPorts();
                foreach (QextPortInfo portInfor, tmp)
                {
                    portsNew << portInfor.portName;
                }
                result = portsNew - portsOld;
                if(result.size() > 0)
                {
                    foundPort = result.toList().at(0);
                    break;
                }
                portsOld = portsNew;

				Sleep::sleep(250);
				elapsed += 250;
            }
#endif
        }

#ifdef Q_OS_WIN32
        QString cmd = getUploadCommand("./Arduino/hardware/tools/avr/bin/avrdude", "./Arduino/hardware/tools/avr/etc/avrdude.conf", mcu, serialPort, baudrate, hexPath_, protocol);
#elif defined(Q_OS_LINUX)
        QString cmd = getUploadCommand("./Arduino/hardware/tools/avrdude", "./Arduino/hardware/tools/avrdude.conf", mcu, QString("/dev/") + serialPort, baudrate, hexPath_, protocol);
#elif defined(Q_OS_MAC)
        QString cmd = getUploadCommand("./Arduino/hardware/tools/avr/bin/avrdude", "./Arduino/hardware/tools/avr/etc/avrdude.conf", mcu, QString("/dev/") + serialPort_, baudrate, hexPath_, protocol);
#endif
        //Sleep::sleep(7000);
        qDebug() << cmd;
        //        pExternalProcess_->execute(cmd);
        if(!QFile::exists(hexPath_))
        {
            return;
        }
        pExternalProcess_->start(cmd);
        pExternalProcess_->waitForFinished(30000);
    }
    else
    {
        qDebug() << "uploader error!";
    }
}

/**
 * @brief 根据传入的文件路径获递归遍历其所有include的文件名字的集合(包括库中包含其他库的所有情况)
 * @param filePath [in] 文件路径
 * @return include的文件名集合
 * @note 注意与getReferenceHeaderFilesFromSingleFile的区别,getReferenceHeaderFilesFromSingleFile是获取一个文本中的#include的文件名.
 * @see 函数 getReferenceHeaderFilesFromSingleFile
 */
QSet<QString> UploadBase::getAllReferenceHeaderFileSet(const QString &filePath)
{
    QSet<QString> libReference;
    libReference = getReferenceHeaderFilesFromSingleFile(filePath);

    foreach (const QString &fileName, libReference.toList())
    {//其实在这个循环之中就过滤了标准的库函数了
        if(map_libName_infor_.contains(fileName))
        {//如果是library
            LibraryReferenceInfor libRefInforInfor = map_libName_infor_.value(fileName);
            libReference += libRefInforInfor.libReference;
        }
        else if(QFileInfo(filePath).baseName() == QFileInfo(fileName).baseName())
        {//处理当前目录 因为.cpp与.h都会包含其他library
            //QString tmpPath =filePath.left(filePath.indexOf(".")) + ".h";
            QString tmpPath;
            if(filePath.contains(".cpp", Qt::CaseInsensitive))
            {
                tmpPath = QString(filePath).replace(QString(".cpp"), QString(".h"));
            }
            else if(filePath.contains(".c", Qt::CaseInsensitive))
            {
                tmpPath = QString(filePath).replace(QString(".c"), QString(".h"));
            }
            else
            {
                qDebug() << "can't find !!!!!!!!!!!";
            }
            if(!alreadyCompile_.contains(tmpPath))
            {
                alreadyCompile_ << tmpPath;
                libReference += getAllReferenceHeaderFileSet(tmpPath);
            }
        }
        else if (map_headerFile_path_.contains(fileName))
        {
            if(map_headerFile_path_.count(fileName) > 1)
            {//处理有重复的情况, 有重复的情况下只处理子目录下的
                QMultiMap<QString, QString>::iterator iter = map_headerFile_path_.find(fileName);
                while (iter != map_headerFile_path_.end()
                       && fileName == iter.key())
                {
                    QString childFilePath =iter.value() + "/" + fileName;
                    QString parentPath = QFileInfo(filePath).path();
                    if(childFilePath.contains(parentPath))
                    {//子目录路径必定包含父级目录的路径
                        if(!alreadyCompile_.contains(childFilePath))
                        {
                            alreadyCompile_ << childFilePath;
                            libReference += getAllReferenceHeaderFileSet(childFilePath);
                        }
                    }
                    ++iter;
                }
            }
            else
            {
                QString childFilePath = map_headerFile_path_.value(fileName) + "/" + fileName;
                if(!alreadyCompile_.contains(childFilePath))
                {
                    alreadyCompile_ << childFilePath;
                    libReference += getAllReferenceHeaderFileSet(childFilePath );
                }
            }
        }
    }

    libReference.remove("Arduino.h");
    return libReference;
}

/**
 * @brief 给定一个文件路径获取其头文件集合
 * @param [in] filePath 文件路径
 * @return 匹配的头文件集合
 * @note 仅处理单一文件, 而getAllReferenceHeaderFileSet则会递归调用
 */
QSet<QString> UploadBase::getReferenceHeaderFilesFromSingleFile(const QString &filePath)
{
    QFile file(filePath);

    if(!file.open(QFile::ReadOnly))
    {
        qDebug() << "getHeaderFiles file can't open " << filePath;
        qDebug() << file.errorString();
        file.close();
        return QSet<QString>();
    }

    QString text = file.readAll();
    QSet<QString> headerFiles = getAllMatchResults(text);
    headerFiles.remove("Arduino.h");

    file.close();

    return headerFiles;
}

/**
 * @brief 获取所有的匹配结果
 * @param text 要匹配的文本
 * @param regexp 正则表达式串
 * @return 匹配的结果集
 * @note 目前的正则表达式仅仅匹配*.h,还不太严谨,容易匹配出注释中形如 *.h*的字符串. 不过这是无害的,这些无效的匹配最终会被过滤.
 * @note 主要在getReferenceHeaderFilesFromText函数中使用
 * @see 函数::getReferenceHeaderFilesFromText
 */
QSet<QString> UploadBase::getAllMatchResults(const QString text, const QString regexp)
{
    QSet<QString> resultSet;

    QRegExp rx(regexp);
    int pos = 0;

    while ((pos = rx.indexIn(text, pos)) != -1)
    {
        pos += rx.matchedLength();
        QString result = rx.cap(0);
        resultSet << result;
    }

    resultSet.remove("Arduino.h");
    return resultSet;
}

/**
 * @brief 扫描Library目录下的头文件极其依赖信息
 * @param libraryPath [in] 路径, 需要指定到Arduino的libraries所在路径
 */
void UploadBase::scanAllLibraryHeaderFile(const QString &libraryPath)
{
    QDir dir(libraryPath);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot); // filter . and ..

    foreach (const QString dirName, dir.entryList())
    {
        QString dirPath = libraryPath + "/" + dirName + "/" + dirName + ".h";
        QFile file(dirPath);

        if(!file.open(QFile::ReadOnly))
        {
            qDebug() << "can't open file in path: " << dirPath;
            file.close();
            continue;
        }

        QString code = file.readAll();
        QSet<QString> library = getAllMatchResults(code, "\\w+\\.h");

        if(library.isEmpty())
        {
            qDebug() << tr("file: %1").arg(dirName) <<"not have include other library";
            file.close();
            continue;
        }

        LibraryReferenceInfor libReferInfor;
        libReferInfor.libName = dirName;
        libReferInfor.libPath = libraryPath + "/" + dirName;
        libReferInfor.libReference = library;

        map_libName_infor_.insert(QString(dirName + ".h"), libReferInfor);
    }
}

/**
 * @brief 扫描指定目录中所有的头文件及其路径(包含其子目录)
 * @param [in] path 要扫描的路径(librarys目录)
 */
void UploadBase::scanAllheaderFile(const QString &path)
{
    //处理文件
    {
        QDir dir(path);
        QStringList filter;
        filter << "*.h";
        dir.setNameFilters(filter);
        foreach (const QString &fileName, dir.entryList())
        {
            QString filePath = path + "/" + fileName;
            map_headerFile_path_.insert(fileName, path);
        }
    }
    //处理子目录
    {
        QDir dir(path);
        dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
        foreach (const QString &dirName, dir.entryList())
        {
            QString dirPath = path + "/" + dirName;
            scanAllheaderFile(dirPath);
        }
    }
}

void UploadBase::slotReadyReadStandardOutput()
{
    QString stdOutPut = pExternalProcess_->readAllStandardOutput();
	//cout << stdOutPut.toUtf8().data() << endl;

	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);

	out << qint32(0);

	{
		QJsonDocument jsonDoc = QJsonDocument::fromVariant(getInfor(stdOutPut));
		out << jsonDoc.toJson();
	}

	out.device()->seek(0);
	out << qint32(qint32(bytes.size()) - qint32(sizeof(qint32)));

	if(pNetWork->pTcpSocket_->waitForConnected())
	{
		qDebug() << "waitForConnected() = true";
#ifdef USE_DEBUG
	pNetWork->pFile_->write("waitForConnected() = true");
#endif
	}
	else
	{
		qDebug() << "waitForConnected() = false";
#ifdef USE_DEBUG
	pNetWork->pFile_->write("waitForConnected() = false");
#endif
	}
	pNetWork->pTcpSocket_->write(bytes);
	pNetWork->pTcpSocket_->waitForBytesWritten();
#ifdef USE_DEBUG
	pNetWork->pFile_->write("send data to server");
#endif
	qDebug() << "发送数据";
}

void UploadBase::slotreadyReadStandardError()
{
    QString errorString = pExternalProcess_->readAllStandardError();
	//cerr << errorString.toUtf8().data() << endl;

	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);

	out << qint32(0);

	{
		QJsonDocument jsonDoc = QJsonDocument::fromVariant(getInfor(errorString));
		out << jsonDoc.toJson();
	}

	out.device()->seek(0);
	out << qint32(qint32(bytes.size()) - qint32(sizeof(qint32)));

	if(pNetWork->pTcpSocket_->waitForConnected())
	{
		qDebug() << "waitForConnected() = true";
#ifdef USE_DEBUG
	pNetWork->pFile_->write("waitForConnected() = true");
#endif
	}
	else
	{
		qDebug() << "waitForConnected() = false";
#ifdef USE_DEBUG
	pNetWork->pFile_->write("waitForConnected() = false");
#endif
	}
	pNetWork->pTcpSocket_->write(bytes);
#ifdef USE_DEBUG
	pNetWork->pFile_->write("send data to server");
#endif

	pNetWork->pTcpSocket_->write(bytes);
	pNetWork->pTcpSocket_->waitForBytesWritten();
	qDebug() << "发送数据";
}
