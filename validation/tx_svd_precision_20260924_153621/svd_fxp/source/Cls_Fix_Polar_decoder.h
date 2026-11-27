#ifndef CLS_FIX_POLAR_DECODER_H
#define CLS_FIX_POLAR_DECODER_H

template <class T>
class Cls_Fix_Polar_decoder
{
public:

        Cls_Fix_Polar_decoder()
        {
        UMatrix             = NULL;
        PathMetric          = NULL;
        Channel_LLR_Mem     = NULL;
        Internal_LLR_Mem    = NULL;
        LLR_Pointer_Mem    = NULL;
        PartialSum_Mem    = NULL;
        PS_Pointer_Mem    = NULL;
        PathMetric_Reg    = NULL;
        Path_bit    = NULL;
        Path_index    = NULL;
        AC_Mem    = NULL;
        Bit_decided    = NULL;
        Path_parent    = NULL;
        }
        virtual ~Cls_Fix_Polar_decoder()
        {
        free(UMatrix             );
        free(PathMetric             );
        free(Channel_LLR_Mem             );
        free(Internal_LLR_Mem             );
        free(LLR_Pointer_Mem             );
        free(PartialSum_Mem             );
        free(PS_Pointer_Mem             );
        free(PathMetric_Reg             );
        free(Path_bit             );
        free(Path_index             );
        free(AC_Mem             );
        free(Bit_decided             );
        free(Path_parent             );

        }

    void init();
    void reset();

	int Q1_N ; //= 127; //信道LLR量化最大绝对值
	int Q3_N ; //= 4094; //路径度量值量化最大绝对值

	int L ;//= 8;
	int n ;//= 9;
	int N ;//= 512;
    int L_n ;//= 1; //有效路径数

	int **UMatrix ;//[8][512] ;//= { 0 };
	int *PathMetric ;//[8] ;//= { 0 };
	int *Channel_LLR_Mem;//[512] ;//= { 0 };
	int **Internal_LLR_Mem;//[8][511] ;//= { 0 }; //中间LLR存储
	int **LLR_Pointer_Mem;//[8][9] ;//= { 0 };  //LLR指针存储
	int **PartialSum_Mem;//[8][511] ;//= { 0 };  //部分和存储
	int **PS_Pointer_Mem;//[8][9] ;//= { 0 };  //部分和指针存储
	int *PathMetric_Reg;//[16] ;//= { 0 }; //路径度量值存储
	int **Path_bit;//[8][512] ;//= { 0 }; //路径存储
	int **Path_index;//[8][512] ;//= { 0 };
	int *AC_Mem;//[512] ;//= { 0 };
	int *Bit_decided;//[8] ;//= { 0 }; //路径度量模块判决结果
	int *Path_parent;;//[8] ;//= { 0 }; //路径度量模块给出的父路径

    void initial();
	void LLRSubModule(int i);
    void PathSubModule(int * LLR_judg, int frozen);
	void PartialSumSubModule(int i);
    void SortWithIndex(int* data, int* index, int len);
	void SCL_Decoder(int *decoderinput,int *AC);

	int int_sign(int k)
	{
		if (k >= 0)
			return 1;
		else
			return -1;
	}
	int int_min(int a, int b)
	{
		if (abs(a) >= abs(b))
			return abs(b);
		else
			return abs(a);
	}

	int CRCcheck(int* code, int* CRCpoly,int codelength, int CRClength);
    int fixed_PolarDecoder(int *decodeSection1, int *DeInterlvLLR, int Mode, int *DecodeOut, int *RecodeOut, int *CRCFlag);

};

template <class T>
void Cls_Fix_Polar_decoder<T>::init()
{
    //极化码译码编码参数
    //UMatrix             =  safe_new(UMatrix  , 8          , 512                     ); //zeros(&sequence_c[0], 8, 512);//{ 0 };
    //ProbPointer            =  safe_new(ProbPointer  , 8          , 10                     ); //zeros(&sequence_c[0], 8, 512);//{ 0 };
    Q1_N = 127; //信道LLR量化最大绝对值
	Q3_N = 4094; //路径度量值量化最大绝对值
	L = 8;
	n = 9;
	N = 512;
    L_n = 1; //int L_n = 1; //有效路径数

    UMatrix                =  safe_new(UMatrix  , 8          , 512                     ); //int UMatrix[8][512] = { 0 };
    PathMetric             =  safe_new(PathMetric  , 8                     ); //int PathMetric[8] = { 0 };
    Channel_LLR_Mem        =  safe_new(Channel_LLR_Mem  , 512                     ); //int Channel_LLR_Mem[512] = { 0 };
	Internal_LLR_Mem       =  safe_new(Internal_LLR_Mem  , 8          , 511                     ); //int Internal_LLR_Mem[8][511] = { 0 }; //中间LLR存储
	LLR_Pointer_Mem        =  safe_new(LLR_Pointer_Mem  , 8          , 9                     ); //int LLR_Pointer_Mem[8][9] = { 0 };  //LLR指针存储
	PartialSum_Mem         =  safe_new(PartialSum_Mem  , 8          , 511                     ); //int PartialSum_Mem[8][511] = { 0 };  //部分和存储
    PS_Pointer_Mem         =  safe_new(PS_Pointer_Mem  , 8          , 9                     ); //int PS_Pointer_Mem[8][9] = { 0 };  //部分和指针存储


	PathMetric_Reg         =  safe_new(PathMetric_Reg  , 16                               ); //int PathMetric_Reg[16] = { 0 }; //路径度量值存储
	Path_bit               =  safe_new(Path_bit  , 8          , 512                     ); //int Path_bit[8][512] = { 0 }; //路径存储
	Path_index             =  safe_new(Path_index  , 8          , 512                     ); //int Path_index[8][512] = { 0 };
	AC_Mem                 =  safe_new(AC_Mem  , 512                               ); //int AC_Mem[512] = { 0 };

	Bit_decided             =  safe_new(Bit_decided  , 8                            ); //int Bit_decided[8] ;//= { 0 }; //路径度量模块判决结果
	Path_parent             =  safe_new(Path_parent  , 8                              ); //int Path_parent[8] ;//= { 0 }; //路径度量模块给出的父路径


    reset();
}

template <class T>
void Cls_Fix_Polar_decoder<T>::reset()
{
//极化码译码编码参数
    zeros(UMatrix, 8, 512);//
    zeros(PathMetric,8);
    zeros(Channel_LLR_Mem, 512);//
    zeros(Internal_LLR_Mem, 8, 512);//

    zeros(LLR_Pointer_Mem, 8, 9);// LLR_Pointer_Mem        =  safe_new(LLR_Pointer_Mem  , 8          , 9                     ); //int LLR_Pointer_Mem[8][9] = { 0 };  //LLR指针存储
	zeros(PartialSum_Mem, 8, 511);// PartialSum_Mem         =  safe_new(PartialSum_Mem  , 8          , 511                     ); //int PartialSum_Mem[8][511] = { 0 };  //部分和存储
    zeros(PS_Pointer_Mem, 8, 9);//PS_Pointer_Mem         =  safe_new(PS_Pointer_Mem  , 8          , 9                     ); //int PS_Pointer_Mem[8][9] = { 0 };  //部分和指针存储


	zeros(PathMetric_Reg, 16);//PathMetric_Reg         =  safe_new(PathMetric_Reg  , 16                               ); //int PathMetric_Reg[16] = { 0 }; //路径度量值存储
	zeros(Path_bit, 8, 512);//Path_bit               =  safe_new(Path_bit  , 8          , 512                     ); //int Path_bit[8][512] = { 0 }; //路径存储
	zeros(Path_index, 8, 512);//Path_index             =  safe_new(Path_index  , 8          , 512                     ); //int Path_index[8][512] = { 0 };
	zeros(AC_Mem, 512);//AC_Mem                 =  safe_new(AC_Mem  , 512                               ); //int AC_Mem[512] = { 0 };

	zeros(Bit_decided, 8);//Bit_decided             =  safe_new(Bit_decided  , 8                            ); //int Bit_decided[8] ;//= { 0 }; //路径度量模块判决结果
	zeros(Path_parent, 8);//Path_parent             =  safe_new(Path_parent  , 8                              ); //int Path_parent[8] ;//= { 0 }; //路径度量模块给出的父路径

}

template <class T>
void Cls_Fix_Polar_decoder<T>::initial()
{
	//初始化指针
	for (int i = 0; i < L; i++)
	{
		for (int j = 0; j < n; j++)
		{
			LLR_Pointer_Mem[i][j] = i;
			PS_Pointer_Mem[i][j]  = i;
		}
	}
	//初始中间层LLR值和部分和值
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 511; j++)
		{
			Internal_LLR_Mem[i][j] = 0;
			PartialSum_Mem[i][j] = 0;
		}
	}
	//初始化路径
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 512; j++)
		{
			Path_bit[i][j] = 0;
			Path_index[i][j] = 0;
		}
	}
	//初始化路径度量值
	for (int i = 0; i < 16; i++)
		PathMetric_Reg[i] = 0;

	L_n = 1;
}

template <class T>
void Cls_Fix_Polar_decoder<T>::LLRSubModule(int i)
{
	int s1 = 0; //开始阶段
	//计算开始阶段
	if (i == 0)
		s1 = n;
	else
	{
		//寻找最低为1的位置
		int j = 0;
		for (j = 0; j < n; j++)
		{
			int temp = i;
			if (((temp >> j)&0x0001) != 0)
				break;
		}

		s1 = j + 1;
	}

	int LLR_Rd1[8] = { 0 };
	int LLR_Rd2[8] = { 0 };
	int PartialSum[8] = { 0 };


	for (int LLR_s = s1; LLR_s > 0; LLR_s--)
	{
		int gFunc_en = ((i >>( LLR_s - 1)) & 0x0001);

		int LLR_Pointer[8] = { 0 };

		if (LLR_s < n)
		{
			for (int j = 0; j < 8; j++)
				LLR_Pointer[j] = LLR_Pointer_Mem[j][LLR_s - 1];
		}

		for (int LLR_ps = 1; LLR_ps <= pow(2, (LLR_s - 1)); LLR_ps++)
		{
			if (LLR_s == n)
			{
				for (int j = 0; j < 8; j++)
				{
					LLR_Rd1[j] = Channel_LLR_Mem[2 * LLR_ps - 1 -1];
					LLR_Rd2[j] = Channel_LLR_Mem[2 * LLR_ps - 1];
				}
			}
			else
			{
				int position = N - (1 << (LLR_s + 1)) + 2 * LLR_ps - 1 - 1;

				for (int j = 0; j < 8; j++)
				{
					LLR_Rd1[j] = Internal_LLR_Mem[LLR_Pointer[j]][position];
					LLR_Rd2[j] = Internal_LLR_Mem[LLR_Pointer[j]][position + 1];
				}
			}

			//读部分和
			for (int j = 0; j < 8; j++)
				PartialSum[j] = PartialSum_Mem[j][N - (1 << LLR_s) + LLR_ps - 1];

			int LLR_Wr[8] = { 0 };

			//计算LLR
			if (gFunc_en)
			{
				for (int j = 0; j < 8; j++)
				{
					//g函数
					if (PartialSum[j] == 1)
					{
						LLR_Wr[j] = LLR_Rd2[j] - LLR_Rd1[j];
					}
					else
					{
						LLR_Wr[j] = LLR_Rd2[j] + LLR_Rd1[j];
					}

					//除2操作

					// if (LLR_Wr[j] > 0 && (LLR_Wr[j] % 2 != 0))
					// 	LLR_Wr[j] = (LLR_Wr[j] >> 1) + 1;
					// else
					// 	LLR_Wr[j] = LLR_Wr[j] >> 1;

					if (LLR_Wr[j] > Q1_N)
						LLR_Wr[j] = Q1_N;
					else if(LLR_Wr[j] < -Q1_N)
						LLR_Wr[j] = -Q1_N;

				}
			}
			else
			{
				//f函数
				for (int j = 0; j < 8; j++)
				{
					LLR_Wr[j] = int_sign(LLR_Rd1[j]) * int_sign(LLR_Rd2[j]) * int_min(LLR_Rd1[j], LLR_Rd2[j]);
				}
			}

			//写入LLR值
			for (int j = 0; j < 8; j++)
			{
				Internal_LLR_Mem[j][N - (1 << LLR_s) + LLR_ps - 1] = LLR_Wr[j];
			}

		}

		if (LLR_s != 1)
		{
			for (int j = 0; j < 8; j++)
			{
				LLR_Pointer_Mem[j][LLR_s - 1 - 1] = j;
			}
		}

	}

}

template <class T>
void Cls_Fix_Polar_decoder<T>::SCL_Decoder(int* decoderinput,int *AC)
{
	initial();

	for (int i = 0; i < N; i++)
		Channel_LLR_Mem[i] = decoderinput[i];
	for (int i = 0; i < N; i++)
		AC_Mem[i] = AC[i];

	for (int i = 0; i < N; i++)
	{
		/*---------------------LLR计算---------------------*/
		LLRSubModule(i);
		/*---------------------中间恢复---------------------*/
		//计算除2次数
		// int cnt = 0;
		// for (int j = 0; j < n; j++)
		// {
		// 	if (((i >> j) & 0x0001) == 1)
		// 		cnt++;
		// }
		// //恢复
		// int LLR_judg[8] = { 0 };
		// for (int j = 0; j < 8; j++)
		// {
		// 	LLR_judg[j] = Internal_LLR_Mem[j][N - 1 - 1] * (1 << cnt);
		// }

		//取消除2恢复
		int LLR_judg[8] = { 0 };
		for (int j = 0; j < 8; j++)
		{
			LLR_judg[j] = Internal_LLR_Mem[j][N - 1 - 1];
		}

		/*---------------------路径计算---------------------*/
		PathSubModule(LLR_judg,AC_Mem[i]);

		/*---------------------指针拷贝---------------------*/
		int LLR_Pointer_temp[8][9] = { 0 };
		int PS_Pointer_temp[8][9] = { 0 };

		for (int pathindex = 0; pathindex < L; pathindex++)
		{
			for (int j = 0; j < n; j++)
			{
				LLR_Pointer_temp[pathindex][j] = LLR_Pointer_Mem[pathindex][j];
				PS_Pointer_temp[pathindex][j]  = PS_Pointer_Mem[pathindex][j];
			}
		}

		for (int pathindex = 0; pathindex < L; pathindex++)
		{
			for (int j = 0; j < n; j++)
			{
				LLR_Pointer_Mem[pathindex][j] = LLR_Pointer_temp[Path_parent[pathindex]][j];
				PS_Pointer_Mem[pathindex][j]  = PS_Pointer_temp[Path_parent[pathindex]][j];
			}
		}

		//记录父路径序号与判决比特
		for (int pathindex = 0; pathindex < 8; pathindex++)
		{
			Path_index[pathindex][i] = Path_parent[pathindex];
			Path_bit[pathindex][i] = Bit_decided[pathindex];
		}

		/*---------------------部分和计算---------------------*/
		PartialSumSubModule(i);
	}

	/*---------------------回溯---------------------*/
	int Path_i[8] = { 0 };
	for (int i = 0; i < L; i++)
		Path_i[i] = i;

	for (int i = N; i >= 1; i--)
	{
		for (int j = 0; j < L; j++)
		{
			UMatrix[j][i - 1] = Path_bit[Path_i[j]][i - 1];
			Path_i[j] = Path_index[Path_i[j]][i - 1];
		}
	}

	for (int i = 0; i < L; i++)
		PathMetric[i] = PathMetric_Reg[i];


}

template <class T>
void Cls_Fix_Polar_decoder<T>::SortWithIndex(int* data, int* index, int len)
{
	int i = 0, j = 0;
	int temp = 0;
	int indextemp = 0;

	for (i = 0; i < len - 1; i++)
	{
		for (j = 0; j < (len - i - 1); j++)
		{
			if (data[j] > data[j + 1])
			{
				temp = data[j];
				indextemp = index[j];

				data[j] = data[j + 1];
				index[j] = index[j + 1];

				data[j + 1] = temp;
				index[j + 1] = indextemp;
			}
		}
	}
}

template <class T>
void Cls_Fix_Polar_decoder<T>::PathSubModule(int* LLR_judg,int frozen)
{
	int PathMetric_Reg_least[16] = { 0 };

	/*---------------------------计算PM值---------------------------*/
	if (frozen)
	{
		for (int i = 0; i < 16; i++)
			PathMetric_Reg_least[i] = PathMetric_Reg[i];

		//计算对应的路径度量值
		for (int i = 1; i <= L_n; i++)
		{
			//当LLR > 0时，判决比特为0
			if (LLR_judg[i - 1] > 0)
			{
				PathMetric_Reg[2*i-1-1]= PathMetric_Reg_least[i-1];
				if (PathMetric_Reg[2 * i - 1 - 1] > Q3_N)
					PathMetric_Reg[2 * i - 1 - 1] = Q3_N;

				PathMetric_Reg[2 * i - 1] = Q3_N + 1;

			}
			// 当LLR < 0时，判决比特为1
			else
			{
				PathMetric_Reg[2 * i - 1 - 1] = PathMetric_Reg_least[i - 1] + abs(LLR_judg[i - 1]);
				if (PathMetric_Reg[2 * i - 1 - 1] > Q3_N)
					PathMetric_Reg[2 * i - 1 - 1] = Q3_N;
				PathMetric_Reg[2 * i - 1] = Q3_N + 1;
			}
		}
	}
	else
	{
		for (int i = 0; i < 16; i++)
			PathMetric_Reg_least[i] = PathMetric_Reg[i];

		//计算对应的路径度量值
		for (int i = 1; i <= L_n; i++)
		{
			if (LLR_judg[i - 1] > 0)
			{
				PathMetric_Reg[2 * i - 1 - 1] = PathMetric_Reg_least[i - 1];
				PathMetric_Reg[2 * i - 1] = PathMetric_Reg_least[i - 1] + abs(LLR_judg[i - 1]);

				if (PathMetric_Reg[2 * i - 1 - 1] > Q3_N)
					PathMetric_Reg[2 * i - 1 - 1] = Q3_N;
				if (PathMetric_Reg[2 * i - 1] > Q3_N)
					PathMetric_Reg[2 * i - 1] = Q3_N;
			}
			else
			{
				PathMetric_Reg[2 * i - 1 - 1] = PathMetric_Reg_least[i - 1] + abs(LLR_judg[i - 1]);
				PathMetric_Reg[2 * i - 1] = PathMetric_Reg_least[i - 1];

				if (PathMetric_Reg[2 * i - 1 - 1] > Q3_N)
					PathMetric_Reg[2 * i - 1 - 1] = Q3_N;
				if (PathMetric_Reg[2 * i - 1] > Q3_N)
					PathMetric_Reg[2 * i - 1] = Q3_N;
			}

		}

	}

	L_n = L_n * 2;
	if (L_n < 2 * L)
	{
		for (int i = L_n + 1; i <= 2 * L ; i++)
		{
			PathMetric_Reg[i - 1] = Q3_N + 2;
		}
	}
	/*---------------------------排序选择---------------------------*/
	if (frozen)
	{
		//排序

		int PathMetric_Reg_temp[16] = { 0 };
		for (int i = 0; i < 16; i++)
			PathMetric_Reg_temp[i] = PathMetric_Reg[i];

		int index[2 * 8] = { 0 };
		for (int indexcnt = 0; indexcnt < 2 * L; indexcnt++)
			index[indexcnt] = indexcnt;

		//quickSortwithIndex(PathMetric_Reg_temp, index, 0, 15);
		SortWithIndex(PathMetric_Reg_temp, index, 16);

		//更新
		for (int i = 0; i < L; i++)
		{
			PathMetric_Reg[i] = PathMetric_Reg_temp[i];
		}

		//计算父路径
		for (int i = 0; i < L; i++)
		{
			Path_parent[i] = index[i] >> 1;
		}
		//判决为全0
		for (int i = 0; i < 8; i++)
			Bit_decided[i] = 0;

		L_n = L_n / 2;
	}
	else
	{
		if (L_n > L)
		{
			//排序
			int PathMetric_Reg_temp[16] = { 0 };
			for (int i = 0; i < 16; i++)
				PathMetric_Reg_temp[i] = PathMetric_Reg[i];

			int index[2 * 8] = { 0 };
			for (int indexcnt = 0; indexcnt < 2 * L; indexcnt++)
				index[indexcnt] = indexcnt;

			//quickSortwithIndex(PathMetric_Reg_temp, index, 0, 15);
			SortWithIndex(PathMetric_Reg_temp, index, 16);
			//更新
			for (int i = 0; i < L; i++)
			{
				PathMetric_Reg[i] = PathMetric_Reg_temp[i];
			}

			//计算父路径
			for (int i = 0; i < L; i++)
			{
				Path_parent[i] = index[i] >> 1;
			}

			//判决
			for (int i = 0; i < L; i++)
			{
				if (index[i] % 2 == 0)
				{
					Bit_decided[i] = 0;
				}
				else
					Bit_decided[i] = 1;
			}

			//路径数减半
			L_n = L_n / 2;
		}
		else
		{
			//排序
			int PathMetric_Reg_temp[16] = { 0 };
			for (int i = 0; i < 16; i++)
				PathMetric_Reg_temp[i] = PathMetric_Reg[i];

			int index[2 * 8] = { 0 };
			for (int indexcnt = 0; indexcnt < 2 * L; indexcnt++)
				index[indexcnt] = indexcnt;

			//quickSortwithIndex(PathMetric_Reg_temp, index, 0, 15);
			SortWithIndex(PathMetric_Reg_temp, index, 16);
			//更新
			for (int i = 0; i < L_n; i++)
			{
				PathMetric_Reg[i] = PathMetric_Reg_temp[i];
			}
			//计算父路径
			for (int i = 0; i < L; i++)
			{
				Path_parent[i] = index[i] >> 1;
			}
			//判决
			for (int i = 0; i < L_n; i++)
			{
				if (index[i] % 2 == 0)
				{
					Bit_decided[i] = 0;
				}
				else
					Bit_decided[i] = 1;
			}
			for (int i = L_n; i < L; i++)
				Bit_decided[i] = 0;

		}


	}
	if (L_n < L)
	{
		for (int i = L_n; i < L; i++)
			Path_parent[i] = 0;

	}


}

template <class T>
void Cls_Fix_Polar_decoder<T>::PartialSumSubModule(int i)
{
	//写入判决结果
	for (int pathindex = 0; pathindex < 8; pathindex++)
		PartialSum_Mem[pathindex][N - 1 - 1] = Bit_decided[pathindex];

	//计算终止阶段
	int s2 = 0;
	if (i == N - 1)
		s2 = 1;
	else if (i == ((N >> 1) - 1))
		s2 = n - 1;
	else
	{
		//寻找最低为1的位置
		int j = 0;
		for (j = 0; j < n; j++)
		{
			int temp = i + 1;
			if (((temp >> j) & 0x0001) != 0)
				break;
		}

		s2 = j + 1;
	}

	for (int PS_s = 1; PS_s <= s2; PS_s++)
	{
		//运算标志位
		int fFunc_en = ((i + 1) >> (PS_s - 1)) & 0x0001;

		//读指针
		int PS_Pointer[8] = { 0 };
		for (int pathindex = 0; pathindex < L; pathindex++)
			PS_Pointer[pathindex] = PS_Pointer_Mem[pathindex][PS_s - 1];

		for (int PS_ps = 1; PS_ps <= (1 << (PS_s - 1)); PS_ps++)
		{
			int PartialSum_Rd1[8] = { 0 };
			int PartialSum_Rd2[8] = { 0 };
			//读部分和
			for (int pathindex = 0; pathindex < L; pathindex++)
				PartialSum_Rd1[pathindex] = PartialSum_Mem[pathindex][N - (1 << PS_s) + PS_ps - 1];

			if (PS_s < n)
			{
				for (int pathindex = 0; pathindex < L; pathindex++)
					PartialSum_Rd2[pathindex] = PartialSum_Mem[PS_Pointer[pathindex]][N - (1 << (PS_s + 1)) + 2 * PS_ps - 1 - 1];
			}
			else
			{
				for (int pathindex = 0; pathindex < L; pathindex++)
					PartialSum_Rd2[pathindex] = 0;
			}

			int PartialSum_Wr1[8] = { 0 };
			int PartialSum_Wr2[8] = { 0 };

			//计算部分和
			if (fFunc_en)
			{
				for (int pathindex = 0; pathindex < L; pathindex++)
				{
					PartialSum_Wr1[pathindex] = PartialSum_Rd1[pathindex];
					PartialSum_Wr2[pathindex] = 0;
				}

			}
			else
			{
				for (int pathindex = 0; pathindex < L; pathindex++)
				{
					PartialSum_Wr1[pathindex] = (PartialSum_Rd1[pathindex] + PartialSum_Rd2[pathindex]) % 2;
					PartialSum_Wr2[pathindex] = PartialSum_Rd1[pathindex];
				}

			}

			//存储部分和
			if (fFunc_en)
			{
				for (int pathindex = 0; pathindex < L; pathindex++)
				{
					PartialSum_Mem[pathindex][N - (1 << (PS_s + 1)) + 2 * PS_ps - 1 - 1] = PartialSum_Wr1[pathindex];
				}
			}
			else
			{
				for (int pathindex = 0; pathindex < L; pathindex++)
				{
					PartialSum_Mem[pathindex][N - (1 << (PS_s + 1)) + 2 * PS_ps - 1 - 1] = PartialSum_Wr1[pathindex];
					PartialSum_Mem[pathindex][N - (1 << (PS_s + 1)) + 2 * PS_ps - 1] = PartialSum_Wr2[pathindex];
				}
			}

			//改写指针
			for (int pathindex = 0; pathindex < L; pathindex++)
				PS_Pointer_Mem[pathindex][PS_s - 1] = pathindex;

		}
	}

}

/* CRC校验函数，校验序列是否通过CRC校验
* code ： 输入待检测序列
* CRCpoly ： 输入生成多项式序列
* codelength : 输入待检测序列长度
* CRClength  ：输入校验比特个数，为生成多项式序列长度减一
* 返回值为校验结果，为0表示校验通过，其他为CRC校验比特错误个数
*/
template <class T>
int Cls_Fix_Polar_decoder<T>::CRCcheck(int* code, int* CRCpoly,int codelength, int CRClength)
{
	int* remainder = NULL;

	remainder = (int*)malloc(sizeof(int) * (CRClength + 1));

	for (int i = 0; i < (CRClength + 1); i++)
		remainder[i] = 0;

	for (int i = 0; i < codelength; i++)
	{
		//移位
		for (int j = CRClength; j > 0; j--)
		{
			remainder[j] = remainder[j - 1];
		}
		remainder[0] = code[i];

		//异或
		if (remainder[CRClength] == 1)
		{
			for (int j = 0; j < (CRClength + 1); j++)
				remainder[j] = remainder[j] ^ CRCpoly[CRClength - j];
		}

	}

	int err = 0;
	for (int i = 0; i < CRClength; i++)
	{
		err = err + remainder[i];
	}

	free(remainder);
	return err;
}
/* 极化码译码再编码函数-定点版本（8位位宽）
* 输入
*	decodeSection1 ： fsk译码后数据，用于第二段译码使用
*	DeInterlvLLR   ： 解交织后的LLR数据
*	Mode           ： 模式（1-联合长码段1译码 ； 2-联合长码段2译码 ； 3-联合短码段1译码 ； 4-联合短码段2译码 ； 5-独立短码译码）
* 输出
*	DecodeOut      ： 译码数据输出
*	RecodeOut      ： 重新编码后输出
*	CRCFlag        ： CRC校验结果（0-通过CRC校验 ； 1 - 未通过CRC校验）
* 不同模式下输入输出长度不同，不需要输入或输出的可传入空指针/NULL
*
*/

template <class T>
int Cls_Fix_Polar_decoder<T>::fixed_PolarDecoder(int *decodeSection1, int *DeInterlvLLR, int Mode, int *DecodeOut, int *RecodeOut, int *CRCFlag)
{
    if (Mode == 1)
    {
		/*--------------------------联合长码段1--------------------------*/
        int decoderinput_temp[512] = {0};
        int decoderinput[512] = {0};
        int BNindex[512] = {0, 256, 128, 384, 64, 320, 192, 448, 32, 288, 160, 416, 96, 352, 224, 480, 16, 272, 144, 400, 80, 336, 208, 464, 48, 304, 176, 432, 112, 368, 240, 496, 8, 264, 136, 392, 72, 328, 200, 456, 40, 296, 168, 424, 104, 360, 232, 488, 24, 280, 152, 408, 88, 344, 216, 472, 56, 312, 184, 440, 120, 376, 248, 504, 4, 260, 132, 388, 68, 324, 196, 452, 36, 292, 164, 420, 100, 356, 228, 484, 20, 276, 148, 404, 84, 340, 212, 468, 52, 308, 180, 436, 116, 372, 244, 500, 12, 268, 140, 396, 76, 332, 204, 460, 44, 300, 172, 428, 108, 364, 236, 492, 28, 284, 156, 412, 92, 348, 220, 476, 60, 316, 188, 444, 124, 380, 252, 508, 2, 258, 130, 386, 66, 322, 194, 450, 34, 290, 162, 418, 98, 354, 226, 482, 18, 274, 146, 402, 82, 338, 210, 466, 50, 306, 178, 434, 114, 370, 242, 498, 10, 266, 138, 394, 74, 330, 202, 458, 42, 298, 170, 426, 106, 362, 234, 490, 26, 282, 154, 410, 90, 346, 218, 474, 58, 314, 186, 442, 122, 378, 250, 506, 6, 262, 134, 390, 70, 326, 198, 454, 38, 294, 166, 422, 102, 358, 230, 486, 22, 278, 150, 406, 86, 342, 214, 470, 54, 310, 182, 438, 118, 374, 246, 502, 14, 270, 142, 398, 78, 334, 206, 462, 46, 302, 174, 430, 110, 366, 238, 494, 30, 286, 158, 414, 94, 350, 222, 478, 62, 318, 190, 446, 126, 382, 254, 510, 1, 257, 129, 385, 65, 321, 193, 449, 33, 289, 161, 417, 97, 353, 225, 481, 17, 273, 145, 401, 81, 337, 209, 465, 49, 305, 177, 433, 113, 369, 241, 497, 9, 265, 137, 393, 73, 329, 201, 457, 41, 297, 169, 425, 105, 361, 233, 489, 25, 281, 153, 409, 89, 345, 217, 473, 57, 313, 185, 441, 121, 377, 249, 505, 5, 261, 133, 389, 69, 325, 197, 453, 37, 293, 165, 421, 101, 357, 229, 485, 21, 277, 149, 405, 85, 341, 213, 469, 53, 309, 181, 437, 117, 373, 245, 501, 13, 269, 141, 397, 77, 333, 205, 461, 45, 301, 173, 429, 109, 365, 237, 493, 29, 285, 157, 413, 93, 349, 221, 477, 61, 317, 189, 445, 125, 381, 253, 509, 3, 259, 131, 387, 67, 323, 195, 451, 35, 291, 163, 419, 99, 355, 227, 483, 19, 275, 147, 403, 83, 339, 211, 467, 51, 307, 179, 435, 115, 371, 243, 499, 11, 267, 139, 395, 75, 331, 203, 459, 43, 299, 171, 427, 107, 363, 235, 491, 27, 283, 155, 411, 91, 347, 219, 475, 59, 315, 187, 443, 123, 379, 251, 507, 7, 263, 135, 391, 71, 327, 199, 455, 39, 295, 167, 423, 103, 359, 231, 487, 23, 279, 151, 407, 87, 343, 215, 471, 55, 311, 183, 439, 119, 375, 247, 503, 15, 271, 143, 399, 79, 335, 207, 463, 47, 303, 175, 431, 111, 367, 239, 495, 31, 287, 159, 415, 95, 351, 223, 479, 63, 319, 191, 447, 127, 383, 255, 511};
        int A[196] = {187, 189, 190, 191, 207, 215, 219, 220, 221, 222, 223, 231, 233, 234, 235, 236, 237, 238, 239, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 287, 303, 311, 313, 314, 315, 316, 317, 318, 319, 334, 335, 339, 341, 342, 343, 345, 346, 347, 348, 349, 350, 351, 355, 357, 358, 359, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383, 391, 395, 397, 398, 399, 403, 404, 405, 406, 407, 408, 409, 410, 411, 412, 413, 414, 415, 417, 418, 419, 420, 421, 422, 423, 424, 425, 426, 427, 428, 429, 430, 431, 432, 433, 434, 435, 436, 437, 438, 439, 440, 441, 442, 443, 444, 445, 446, 447, 449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 460, 461, 462, 463, 464, 465, 466, 467, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477, 478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 495, 496, 497, 498, 499, 500, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511};
        int AC[512] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        //double Decoder_sigma = 0.800607607023767;
        int CRCpoly[] = {1, 1, 1, 0, 0, 1, 0, 1};

        for (int i = 0; i < 512; i++)
            decoderinput_temp[i] = 0;

        for (int i = 128, j = 0; i < 512; i++, j++)
            decoderinput_temp[i] = DeInterlvLLR[j];

        for (int i = 0; i < 512; i++)
            decoderinput[i] = decoderinput_temp[BNindex[i]];

        /*-------------------------SCL译码-------------------------*/
        //fixed_decoder decoder1;


        int FixedWidth = 8;
        Q1_N = (1 << (FixedWidth - 1)) - 1;
        Q3_N = (1 << (FixedWidth + 4)) - 2;

        SCL_Decoder(decoderinput, AC);

        /*-------------------------7bit CRC解码-------------------------*/
        int recode_U[512] = {0};
        int decodeinfotemp[196] = {0};
        int isCRCpassed = 0;
        for (int pathindex = 0; pathindex < 8; pathindex++)
        {
            //取得当前路径中的信息比特
            for (int i = 0; i < 196; i++)
                decodeinfotemp[i] = UMatrix[pathindex][A[i]];

            //检查当前路径是否通过CRC校验
            int err = CRCcheck(decodeinfotemp, CRCpoly, 196, 7);

            //当前路径通过CRC校验
            if (err == 0)
            {
                for (int i = 0; i < 189; i++)
                    DecodeOut[i] = decodeinfotemp[i];
                for (int i = 0; i < 512; i++)
                    recode_U[i] = UMatrix[pathindex][i];

                isCRCpassed = 1;
                *CRCFlag = 0;
                break;
            }
        }
        if (!isCRCpassed)
        {
            for (int i = 0; i < 189; i++)
                DecodeOut[i] = UMatrix[0][A[i]];
            for (int i = 0; i < 512; i++)
                recode_U[i] = UMatrix[0][i];
            *CRCFlag = 1;
        }
        /*-------------------------重新编码-------------------------*/
        //极化码编码,蝶形结构，不使用生成矩阵
        for (int i = 0; i < 9; i++)
        {
            int Step = (int)pow(2, i);
            for (int j = 0; j < 512; j = j + 2 * Step)
            {
                for (int k = 0; k < Step; k++)
                {
                    recode_U[j + k] = (recode_U[j + k] + recode_U[j + k + Step]) % 2;
                    recode_U[j + k + Step] = recode_U[j + k + Step];
                }
            }
        }
        //打孔赋值第一段
        for (int i = 0; i < 384; i++)
            RecodeOut[i] = recode_U[i + 128];
    }
    else if (Mode == 2)
    {
		/*--------------------------联合长码段2--------------------------*/
        int decoderinput_temp[512] = {0};
        int decoderinput[512] = {0};
        int BNindex[512] = {0, 256, 128, 384, 64, 320, 192, 448, 32, 288, 160, 416, 96, 352, 224, 480, 16, 272, 144, 400, 80, 336, 208, 464, 48, 304, 176, 432, 112, 368, 240, 496, 8, 264, 136, 392, 72, 328, 200, 456, 40, 296, 168, 424, 104, 360, 232, 488, 24, 280, 152, 408, 88, 344, 216, 472, 56, 312, 184, 440, 120, 376, 248, 504, 4, 260, 132, 388, 68, 324, 196, 452, 36, 292, 164, 420, 100, 356, 228, 484, 20, 276, 148, 404, 84, 340, 212, 468, 52, 308, 180, 436, 116, 372, 244, 500, 12, 268, 140, 396, 76, 332, 204, 460, 44, 300, 172, 428, 108, 364, 236, 492, 28, 284, 156, 412, 92, 348, 220, 476, 60, 316, 188, 444, 124, 380, 252, 508, 2, 258, 130, 386, 66, 322, 194, 450, 34, 290, 162, 418, 98, 354, 226, 482, 18, 274, 146, 402, 82, 338, 210, 466, 50, 306, 178, 434, 114, 370, 242, 498, 10, 266, 138, 394, 74, 330, 202, 458, 42, 298, 170, 426, 106, 362, 234, 490, 26, 282, 154, 410, 90, 346, 218, 474, 58, 314, 186, 442, 122, 378, 250, 506, 6, 262, 134, 390, 70, 326, 198, 454, 38, 294, 166, 422, 102, 358, 230, 486, 22, 278, 150, 406, 86, 342, 214, 470, 54, 310, 182, 438, 118, 374, 246, 502, 14, 270, 142, 398, 78, 334, 206, 462, 46, 302, 174, 430, 110, 366, 238, 494, 30, 286, 158, 414, 94, 350, 222, 478, 62, 318, 190, 446, 126, 382, 254, 510, 1, 257, 129, 385, 65, 321, 193, 449, 33, 289, 161, 417, 97, 353, 225, 481, 17, 273, 145, 401, 81, 337, 209, 465, 49, 305, 177, 433, 113, 369, 241, 497, 9, 265, 137, 393, 73, 329, 201, 457, 41, 297, 169, 425, 105, 361, 233, 489, 25, 281, 153, 409, 89, 345, 217, 473, 57, 313, 185, 441, 121, 377, 249, 505, 5, 261, 133, 389, 69, 325, 197, 453, 37, 293, 165, 421, 101, 357, 229, 485, 21, 277, 149, 405, 85, 341, 213, 469, 53, 309, 181, 437, 117, 373, 245, 501, 13, 269, 141, 397, 77, 333, 205, 461, 45, 301, 173, 429, 109, 365, 237, 493, 29, 285, 157, 413, 93, 349, 221, 477, 61, 317, 189, 445, 125, 381, 253, 509, 3, 259, 131, 387, 67, 323, 195, 451, 35, 291, 163, 419, 99, 355, 227, 483, 19, 275, 147, 403, 83, 339, 211, 467, 51, 307, 179, 435, 115, 371, 243, 499, 11, 267, 139, 395, 75, 331, 203, 459, 43, 299, 171, 427, 107, 363, 235, 491, 27, 283, 155, 411, 91, 347, 219, 475, 59, 315, 187, 443, 123, 379, 251, 507, 7, 263, 135, 391, 71, 327, 199, 455, 39, 295, 167, 423, 103, 359, 231, 487, 23, 279, 151, 407, 87, 343, 215, 471, 55, 311, 183, 439, 119, 375, 247, 503, 15, 271, 143, 399, 79, 335, 207, 463, 47, 303, 175, 431, 111, 367, 239, 495, 31, 287, 159, 415, 95, 351, 223, 479, 63, 319, 191, 447, 127, 383, 255, 511};
        int A[196] = {187, 189, 190, 191, 207, 215, 219, 220, 221, 222, 223, 231, 233, 234, 235, 236, 237, 238, 239, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 287, 303, 311, 313, 314, 315, 316, 317, 318, 319, 334, 335, 339, 341, 342, 343, 345, 346, 347, 348, 349, 350, 351, 355, 357, 358, 359, 361, 362, 363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378, 379, 380, 381, 382, 383, 391, 395, 397, 398, 399, 403, 404, 405, 406, 407, 408, 409, 410, 411, 412, 413, 414, 415, 417, 418, 419, 420, 421, 422, 423, 424, 425, 426, 427, 428, 429, 430, 431, 432, 433, 434, 435, 436, 437, 438, 439, 440, 441, 442, 443, 444, 445, 446, 447, 449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 460, 461, 462, 463, 464, 465, 466, 467, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477, 478, 479, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 495, 496, 497, 498, 499, 500, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511};
        int AC[512] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        //double Decoder_sigma = 0.800607607023767;
        int CRCpoly[] = {1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1};

        /*-------------------------补零 比特翻转-------------------------*/
        for (int i = 0; i < 512; i++)
            decoderinput_temp[i] = 0;

        for (int i = 128, j = 0; i < 512; i++, j++)
            decoderinput_temp[i] = DeInterlvLLR[j];

        for (int i = 0; i < 512; i++)
            decoderinput[i] = decoderinput_temp[BNindex[i]];

        /*-------------------------SCL译码-------------------------*/

        int FixedWidth = 8;
        Q1_N = (1 << (FixedWidth - 1)) - 1;
        Q3_N = (1 << (FixedWidth + 4)) - 2;

        SCL_Decoder(decoderinput, AC);

        /*-------------------------15bit CRC解码-------------------------*/
        int decodeinfotemp[385] = {0};
        int isCRCpassed = 0;

        for (int pathindex = 0; pathindex < 8; pathindex++)
        {
            //取得当前路径中的信息比特
            //第一段数据
            for (int i = 0; i < 189; i++)
                decodeinfotemp[i] = decodeSection1[i];
            //第二段数据
            for (int i = 0, j = 189; i < 196; i++, j++)
                decodeinfotemp[j] = UMatrix[pathindex][A[i]];

            //检查当前路径是否通过CRC校验
            int err = CRCcheck(decodeinfotemp, CRCpoly, 385, 15);

            //当前路径通过CRC校验
            if (err == 0)
            {
                for (int i = 189, j = 0; i < 370; i++, j++)
                    DecodeOut[j] = decodeinfotemp[i];

                isCRCpassed = 1;
                *CRCFlag = 0;
                break;
            }
        }
        if (!isCRCpassed)
        {
            for (int i = 0; i < 181; i++)
                DecodeOut[i] = UMatrix[0][A[i]];

            *CRCFlag = 1;
        }
    }
    else if (Mode == 3)
    {
		/*--------------------------联合短码段1--------------------------*/
        int decoderinput_temp[256] = {0};
        int decoderinput[256] = {0};
        int A[108] = {63, 93, 94, 95, 103, 107, 109, 110, 111, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 143, 151, 155, 157, 158, 159, 167, 170, 171, 172, 173, 174, 175, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 195, 197, 198, 199, 201, 202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};
        int AC[256] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int BNindex[256] = {0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240, 8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248, 4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244, 12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252, 2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242, 10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250, 6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246, 14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254, 1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241, 9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249, 5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245, 13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253, 3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243, 11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251, 7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247, 15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};
        //double Decoder_sigma = 0.822490333512213;
        int CRCpoly[] = {1, 1, 1, 0, 0, 1, 0, 1};

        /*-------------------------补零 比特翻转-------------------------*/
        for (int i = 0; i < 256; i++)
            decoderinput_temp[i] = 0;

        for (int i = 48, j = 0; i < 256; i++, j++)
            decoderinput_temp[i] = DeInterlvLLR[j];

        for (int i = 0; i < 256; i++)
            decoderinput[i] = decoderinput_temp[BNindex[i]];

        /*-------------------------SCL译码-------------------------*/

        int FixedWidth = 8;
        n = 8;
        N = 256;
        Q1_N = (1 << (FixedWidth - 1)) - 1;
        Q3_N = (1 << (FixedWidth + 4)) - 2;

        SCL_Decoder(decoderinput, AC);

        /*-------------------------7bit CRC解码-------------------------*/
        int recode_U[512] = {0};
        int decodeinfotemp[108] = {0};
        int isCRCpassed = 0;
        for (int pathindex = 0; pathindex < 8; pathindex++)
        {
            //取得当前路径中的信息比特
            for (int i = 0; i < 108; i++)
                decodeinfotemp[i] = UMatrix[pathindex][A[i]];

            //检查当前路径是否通过CRC校验
            int err = CRCcheck(decodeinfotemp, CRCpoly, 108, 7);

            //当前路径通过CRC校验
            if (err == 0)
            {
                for (int i = 0; i < 101; i++)
                    DecodeOut[i] = decodeinfotemp[i];
                for (int i = 0; i < 256; i++)
                    recode_U[i] = UMatrix[pathindex][i];

                isCRCpassed = 1;
                *CRCFlag = 0;
                break;
            }
        }
        if (!isCRCpassed)
        {
            for (int i = 0; i < 101; i++)
                DecodeOut[i] = UMatrix[0][A[i]];
            for (int i = 0; i < 256; i++)
                recode_U[i] = UMatrix[0][i];
            *CRCFlag = 1;
        }
        /*-------------------------重新编码-------------------------*/
        for (int i = 256; i < 512; i++)
            recode_U[i] = 0;
        //极化码编码,蝶形结构，不使用生成矩阵
        for (int i = 0; i < 9; i++)
        {
            int Step = (int)pow(2, i);
            for (int j = 0; j < 512; j = j + 2 * Step)
            {
                for (int k = 0; k < Step; k++)
                {
                    recode_U[j + k] = (recode_U[j + k] + recode_U[j + k + Step]) % 2;
                    recode_U[j + k + Step] = recode_U[j + k + Step];
                }
            }
        }
        //打孔赋值第一段
        for (int i = 0; i < 208; i++)
            RecodeOut[i] = recode_U[i + 48];
    }
    else if (Mode == 4)
    {
		/*--------------------------联合短码段2--------------------------*/
        int decoderinput_temp[256] = {0};
        int decoderinput[256] = {0};
        int A[108] = {63, 93, 94, 95, 103, 107, 109, 110, 111, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 143, 151, 155, 157, 158, 159, 167, 170, 171, 172, 173, 174, 175, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 195, 197, 198, 199, 201, 202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};
        int AC[256] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int BNindex[256] = {0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240, 8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248, 4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244, 12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252, 2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242, 10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250, 6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246, 14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254, 1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241, 9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249, 5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245, 13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253, 3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243, 11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251, 7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247, 15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};
        //double Decoder_sigma = 0.822490333512213;
        int CRCpoly[] = {1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1};

        /*-------------------------补零 比特翻转-------------------------*/
        for (int i = 0; i < 256; i++)
            decoderinput_temp[i] = 0;

        for (int i = 48, j = 0; i < 256; i++, j++)
            decoderinput_temp[i] = DeInterlvLLR[j];

        for (int i = 0; i < 256; i++)
            decoderinput[i] = decoderinput_temp[BNindex[i]];

        /*-------------------------SCL译码-------------------------*/
        //
        int FixedWidth = 8;
        n = 8;
        N = 256;
        Q1_N = (1 << (FixedWidth - 1)) - 1;
        Q3_N = (1 << (FixedWidth + 4)) - 2;
        SCL_Decoder(decoderinput, AC);

        /*-------------------------15bit CRC解码-------------------------*/
        int decodeinfotemp[209] = {0};
        int isCRCpassed = 0;
        for (int pathindex = 0; pathindex < 8; pathindex++)
        {
            //取得当前路径中的信息比特
            //第一段数据
            for (int i = 0; i < 101; i++)
                decodeinfotemp[i] = decodeSection1[i];
            //第二段数据
            for (int i = 0, j = 101; i < 108; i++, j++)
                decodeinfotemp[j] = UMatrix[pathindex][A[i]];

            //检查当前路径是否通过CRC校验
            int err = CRCcheck(decodeinfotemp, CRCpoly, 209, 15);

            //当前路径通过CRC校验
            if (err == 0)
            {
                for (int i = 101, j = 0; i < 194; i++, j++)
                    DecodeOut[j] = decodeinfotemp[i];

                isCRCpassed = 1;
                *CRCFlag = 0;
                break;
            }
        }
        if (!isCRCpassed)
        {
            for (int i = 0; i < 93; i++)
                DecodeOut[i] = UMatrix[0][A[i]];
            *CRCFlag = 1;
        }
    }
    else if (Mode == 5)
    {
		/*--------------------------独立短码--------------------------*/
        int decoderinput_temp[256] = {0};
        int decoderinput[256] = {0};
        int A[108] = {63, 93, 94, 95, 103, 107, 109, 110, 111, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 143, 151, 155, 157, 158, 159, 167, 170, 171, 172, 173, 174, 175, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 195, 197, 198, 199, 201, 202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};
        int AC[256] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int BNindex[256] = {0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240, 8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248, 4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244, 12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252, 2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242, 10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250, 6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246, 14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254, 1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241, 9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249, 5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245, 13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253, 3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243, 11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251, 7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247, 15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};
        //double Decoder_sigma = 0.822490333512213;
        int CRCpoly[] = {1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1};

        /*-------------------------补零 比特翻转-------------------------*/
        for (int i = 0; i < 256; i++)
            decoderinput_temp[i] = 0;

        for (int i = 48, j = 0; i < 256; i++, j++)
            decoderinput_temp[i] = DeInterlvLLR[j];

        for (int i = 0; i < 256; i++)
            decoderinput[i] = decoderinput_temp[BNindex[i]];

        /*-------------------------SCL译码-------------------------*/

        int FixedWidth = 8;
        n = 8;
        N = 256;
        Q1_N = (1 << (FixedWidth - 1)) - 1;
        Q3_N = (1 << (FixedWidth + 4)) - 2;

        SCL_Decoder(decoderinput, AC);

        /*-------------------------11bit CRC解码-------------------------*/
        int decodeinfotemp[108] = {0};
        int isCRCpassed = 0;
        for (int pathindex = 0; pathindex < 8; pathindex++)
        {
            //取得当前路径中的信息比特
            for (int i = 0; i < 108; i++)
                decodeinfotemp[i] = UMatrix[pathindex][A[i]];

            //检查当前路径是否通过CRC校验
            int err = CRCcheck(decodeinfotemp, CRCpoly, 108, 11);

            //当前路径通过CRC校验
            if (err == 0)
            {
                for (int i = 0; i < 97; i++)
                    DecodeOut[i] = decodeinfotemp[i];

                isCRCpassed = 1;
                *CRCFlag = 0;
                break;
            }
        }

        if (!isCRCpassed)
        {
            for (int i = 0; i < 97; i++)
                DecodeOut[i] = UMatrix[0][A[i]];

            *CRCFlag = 1;
        }
    }
    else
        return 1;

    return 0;
}


#endif
