/**
 * @addtogroup module_genericGateway
 * @{
 */

/**
 * @file
 * @brief USB������������USB��μ����ء�
 * @details
 * @version 1.0.0
 * @author sky.houfei
 * @date 2016-3-18
 */

//******************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "UsbController.h"
#include "GenericGateway.h"

//******************************************************
#define UEVENT_BUFFER_SIZE 2048

//******************************************************
static bool isUsbConnected = false;
static int s_hotplugSock = 0;
static bool s_isMounted = false;

 //******************************************************
static void* UsbController_HotPlugMonitor(void);  // USB����������USB�Ĳ���¼��������й��غ�ж��USB�豸��


//******************************************************
static int UsbController_HotplugSockInit(void)
{
    const int buffersize = 1024;
    int ret;

    struct sockaddr_nl snl;
    bzero(&snl, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;

    int s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (s == -1)
    {
        perror("socket");
        return -1;
    }
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize));

    ret = bind(s, (struct sockaddr *)&snl, sizeof(struct sockaddr_nl));
    if (ret < 0)
    {
        perror("bind");
        close(s);
        return -1;
    }

    return s;
}


/**
 * @brief USB��������ʼ����׼��USB�ļ�������
 * @return ret, int�������ʼ���ɹ����򷵻�0������Ϊ-1.
 */
int UsbController_Init(void)
{
    const int buffersize = 1024;
    int ret;
    pthread_t id;

    struct sockaddr_nl snl;
    bzero(&snl, sizeof(struct sockaddr_nl));
    snl.nl_family = AF_NETLINK;
    snl.nl_pid = getpid();
    snl.nl_groups = 1;

    if (access("/dev/sda1", 0) == 0)
    {
        // USB�Ѿ����ӳɹ�
        isUsbConnected = true;
    }


    UsbController_MountMonitor();   // �״μ��USB�Ƿ����
    s_hotplugSock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (s_hotplugSock == -1)
    {
        perror("socket error");
        return -1;
    }
    setsockopt(s_hotplugSock, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize));

    ret = bind(s_hotplugSock, (struct sockaddr *)&snl, sizeof(struct sockaddr_nl));
    if (ret < 0)
    {
        perror("bind error");
        close(s_hotplugSock);
        return -1;
    }

    ret = pthread_create(&id, NULL, UsbController_HotPlugMonitor, NULL);
    if (ret != 0)
    {
        printf("pthread_create error = %d\n", ret);
    }

    return 0;
}


/**
 * @brief USB�����Ȳ�Σ�����USB�Ĳ���¼��������й��غ�ж��USB�豸��
 */
static void* UsbController_HotPlugMonitor(void)
{
    pthread_detach(pthread_self());
    char *result = NULL;
    char buf[UEVENT_BUFFER_SIZE * 2] = {0};     //  Netlink message buffer

    while (1)
    {
        recv(s_hotplugSock, &buf, sizeof(buf), 0); // ��ȡ USB �豸�Ĳ�λ�����ַ���Ϣ��
        result = strtok(buf, "@");                // �鿴 USB�Ĳ��뻹�ǰγ���Ϣ
        if (result != NULL)
        {
            if ((strcmp(result, "add") == 0))
            {
                if (isUsbConnected == false)
                {
                    isUsbConnected = true;
                }

            }
            else if ((strcmp(result, "remove") == 0))
            {
                if (isUsbConnected == true)
                {
                    isUsbConnected = false;
                }
            }
        }
        memset(buf, 0, UEVENT_BUFFER_SIZE * 2);
    }
}


/**
* @brief �Ƿ����ӳɹ���
* @return bool isConnnected, USB�豸���ӳɹ����򷵻� true, ���򷵻�false��
*/
static bool UsbController_IsConnected(void)
{
    return isUsbConnected;
}


/**
 * @brief �����ļ�ϵͳ��
 * @details �����ļ��� /tmp/usb����USB�豸�����ڸ�Ŀ¼�¡����Թ��� sda1��sdb1�����������ʧ�ܣ�����Ϊ����ʧ�ܡ�
 * @return ������سɹ����򷵻�0������Ϊ-1��
 */
static int UsbController_MountFileSystem(void)
{
    const char directory[] = "/tmp/usb";
    int ret = 0;

    printf("Try to mount the usb device\n");
    // ����Ƿ�����ļ���
    if (access(directory, 0) == -1)
    {
        // �ļ��в�����
        if (mkdir(directory, 0777)) // �����ļ���
        {
            printf("creat directory(%s) failed!!!", directory);
            return -1;
        }
    }

    if (system("mount -t vfat /dev/sda1  /tmp/usb") < 0)  // ����USB���ļ�ϵͳ
    {
        if (system("mount -t vfat /dev/sdb1  /tmp/usb") < 0)
        {
            return -1;
        }
    }

    return 0;
}


/**
 * @brief ж���ļ�ϵͳ��
 * @return ������سɹ����򷵻�0������Ϊ-1��
 */
static int UsbController_UnmountFileSystem(void)
{
    int ret = 0;

    if (system("umount /tmp/usb") < 0)  // ����USB���ļ�ϵͳ
    {
        printf("Umount the usb device failed\n");
        ret =  -1;
    }

    printf("Umount the usb device success\n");
    return ret;
}


/**
 * @brief USB�豸�Ƿ���Թ��ء�
 * @details �豸��������״̬������/dev/Ŀ¼�´�����sda1����sdb1�ڵ㣬����Ϊ���Թ��ء�
 * @return ������Թ��ڣ��򷵻�true������Ϊfalse��
 */
static bool UsbController_IsMountable(void)
{
    bool isMountable = false;
    bool isPartitionExist = false;

    if (access("/dev/sda1", 0) == 0 || access("/dev/sdb1", 0) == 0)
    {
        // ���ڷ��� /dev/sda1 ���� /dev/sdb1
        isPartitionExist = true;
    }

    if (isUsbConnected == true && isPartitionExist == true)
    {
        isMountable = true;
    }

    return isMountable;
}


/**
 * @brief USB�豸���ؼ�����
 * @details ���USB֮ǰû�й����ҵ�ǰ���Թ��أ�����ء�
 * \n ���USB֮ǰ���سɹ�����ʱ�豸�Ѿ����γ�����ж�ء�
 */
void UsbController_MountMonitor(void)
{
    if (s_isMounted == false && UsbController_IsMountable() == true)
    {
        // ֮ǰû�й����ҵ�ǰ���Թ��أ������ļ�ϵͳ
        if (0 == UsbController_MountFileSystem())
        {
            printf("Mount success\n");
            s_isMounted = true;
            GenericGateway_SetUsbMounted(s_isMounted);
        }
    }
    else if (s_isMounted == true && UsbController_IsConnected() == false)
    {
        // ֮ǰ���سɹ�����ʱ�豸�Ѿ����γ���ж���豸
        if (0 == UsbController_UnmountFileSystem())
        {
            s_isMounted = false;
            GenericGateway_SetUsbMounted(s_isMounted);
        }
    }
}


/**
* @brief �Ƿ��Ѿ����سɹ���
* @return bool s_isMounted, USB�豸���سɹ����򷵻� true, ���򷵻�false��
*/
bool UsbController_IsMounted(void)
{
    return s_isMounted;
}


/** @} */

