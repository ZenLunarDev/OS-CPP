#ifndef USERINFO_H
#define USERINFO_H

// ตำแหน่ง Logical Address สำหรับ User Process
// (ต่อให้ physical อยู่ไหนก็ตาม ประสบการณ์ของ Process คือ address นี้)
#define USER_BASE 0x40000000
#define USER_REGION_SIZE (3 * 1024 * 1024)   // 3MB
#define USER_STACK_TOP (USER_BASE + USER_REGION_SIZE)  // 0x40300000
#define USER_STACK_SIZE 0x10000

#endif
