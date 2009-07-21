/*
 *  ioctl.c - ������ ���������, ������������ ioctl ��� ���������� ������� ����
 *
 *  �� ��� ��� �� ����������� �������� cat, ��� �������� ������ �/�� ������.
 *  ������ �� �� ������ �������� ���� ���������, ������� ������������ �� ioctl.
 */

/* 
 * ����������� �������� ������ ���������� � ���� �������� ioctl
 */
#include "charbridge.h"

#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <errno.h>

/* 
 * ������� ������ � ��������� ����� ioctl  
 */

ioctl_set_msg(int file_desc, char *message)
{
  int ret_val;

  ret_val = ioctl(file_desc, IOCTL_SET_MSG, message);

  if (ret_val < 0) {
    printf("������ ��� ������ ioctl_set_msg: %d\n", ret_val);
    exit(-1);
  }
}

ioctl_get_msg(int file_desc)
{
  int ret_val;
  char message[100];

  /* 
   * �������� - ���� ������� �� ����� -- ����� ����� ����� �� ����������
   * ������� �������� ������, ��������� � ������������� ������.
   * � �������� �������� ��� ���������� �������������
   * �������� � ioctl ���� �������������� ����������:
   * ���������� ������ ��������� � ��� �����
   */
  ret_val = ioctl(file_desc, IOCTL_GET_MSG, message,sizeof(message));

  if (ret_val < 0) {
    printf("������ ��� ������ ioctl_get_msg: %d\n", ret_val);
    exit(-1);
  }

  printf("�������� ��������� (get_msg): %s\n", message);
}

ioctl_get_nth_byte(int file_desc)
{
  int i;
  char c;

  printf("N-��� ���� � ��������� (get_nth_byte): ");

  i = 0;
  while (c != 0) {
    c = ioctl(file_desc, IOCTL_GET_NTH_BYTE, i++);

    if (c < 0) {
      printf
        ("������ ��� ������ ioctl_get_nth_byte �� %d-� �����.\n", i);
      exit(-1);
    }

    putchar(c);
  }
  putchar('\n');
}

/* 
 * Main - �������� ����������������� ������� ioctl
 */
main()
{
  int file_desc, ret_val;
  char *msg = "��� ��������� ���������� ����� ioctl\n";

//  file_desc = open(DEVICE_FILE_NAME, 0);
  file_desc = open("/dev/cbsideA", 0);
  if (file_desc < 0) {
    printf("���������� ������� ���� ����������: %s\n", DEVICE_FILE_NAME);
	printf("With error(%d): %s\n", errno, strerror(errno));
    exit(-1);
  }

  ioctl_set_msg(file_desc, msg);
  ioctl_get_msg(file_desc);
  ioctl_get_nth_byte(file_desc);
  close(file_desc);
}
