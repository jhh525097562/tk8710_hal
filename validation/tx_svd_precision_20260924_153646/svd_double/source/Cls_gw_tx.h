/****************************************************************************
**
**  Copyright  (c)  2023-2025,  All Rights Reserved.
**
**  File          : Cls_gw_tx.h
**  Created By    : Darren.Ren
**  Created On    : 16/10/2023
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice: 配置层, 主要功能实体，把配置从命令行或者文件读到channel和tx/rx中
**
****************************************************************************/

#ifndef __gw_tx
#define __gw_tx

#include "common_type.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>

class Cls_gw_tx
{
	public:
		void init(int argc, char *argv[]);
		void go(){;} 
	private:
		
};

void Cls_gw_tx::init(int argc, char *argv[])
{

}



#endif