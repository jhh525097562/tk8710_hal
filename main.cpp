/****************************************************************************
**
**  Copyright  (c)  2023-2025,  All Rights Reserved.
**
**  File          : main.cpp (main program to run simulation)
**  Created By    : Darren.Ren
**  Created On    : 11/24/2019
**
**  History     :
**  Programmer    Date         Rev       Description
**  --------------- ---------- -------- ------------------------------
**
**  Notice: 主函数入口
****************************************************************************/

#include "E:\K2301Cmodel\c_model\Cls_sim.h"

int main(int argc, char *argv[])
{
	Cls_sim platform;

	if(!platform.init(argc, argv)) return 0;

	platform.go();
	LOG2DEV << "Finished" << endl;
	return 0;
}
