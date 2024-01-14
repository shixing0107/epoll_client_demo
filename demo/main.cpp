#include <QCoreApplication>
#include "rslogging.h"
#include "tcpipsercnt.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    TcpIp tcpIp;
    return a.exec();
}
