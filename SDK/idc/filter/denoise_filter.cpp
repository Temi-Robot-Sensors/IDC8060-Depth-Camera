#include "denoise_filter.h"
#include "string.h"

const int DenoiseFilter::dx1[8] = { -1,-1,-1,0,0,1,1,1 };
const int DenoiseFilter::dy1[8] = { -1,0,1,-1,1,-1,0,1 };
const int DenoiseFilter::dx2[24] = { -2,-2,-2,-2,-2,-1,-1,-1,-1,-1,0,0,0,0,1,1,1,1,1,2,2,2,2,2 };
const int DenoiseFilter::dy2[24] = { -2,-1,0,1,2,-2,-1,0,1,2,-2,-1,1,2,-2,-1,0,1,2,-2,-1,0,1,2 };

float DenoiseFilter::Big[25] =
{ 0.15f,0.2f,0.15f,0.2f,0.15f,
0.2f,0.4f,0.4f,0.4f,0.2f,
0.25f,0.4f,0.5f,0.4f,0.25f,
0.2f,0.4f,0.4f,0.4f,0.2f,
0.15f,0.2f,0.15f,0.2f,0.15f };

float DenoiseFilter::Sma[25] =
{ 0.05f,0.15f,0.15f,0.15f,0.05f,
0.1f,0.2f,0.25f,0.2f,0.1f,
0.15f,0.25f,0.7f,0.25f,0.15f,
0.1f,0.2f,0.25f,0.2f,0.1f,
0.05f,0.15f,0.15f,0.15f,0.05f };

float DenoiseFilter::Dis2D[25] =
{ 1.0f / 2.828427125f,1.0f / 2.236067977f,1.0f / 2.0f,1.0f / 2.236067977f,1.0f / 2.828427125f,
1.0f / 2.236067977f,1.0f / 1.414213562f,1.0f / 1.0f,1.0f / 1.414213562f,1.0f / 2.236067977f,
1.0f / 2.0f,1.0f / 1.0f, 2.5f,1.0f / 1.0f,1.0f / 2.0f,
1.0f / 2.236067977f,1.0f / 1.414213562f,1.0f / 1.0f,1.0f / 1.414213562f,1.0f / 2.236067977f,
1.0f / 2.828427125f,1.0f / 2.236067977f,1.0f / 2.0f,1.0f / 2.236067977f,1.0f / 2.828427125f };

float DenoiseFilter::One[25] =
{ 1.0f,1.0f,1.0f,1.0f,1.0f,
1.0f,1.0f,1.0f,1.0f,1.0f,
1.0f,1.0f,0.0f,1.0f,1.0f,
1.0f,1.0f,1.0f,1.0f,1.0f,
1.0f,1.0f,1.0f,1.0f,1.0f };

Matrix DenoiseFilter::OneMat(5, 5, One);
Matrix DenoiseFilter::BigMat(5, 5, Big);
Matrix DenoiseFilter::SmaMat(5, 5, Sma);
Matrix DenoiseFilter::Dis2DMat(5, 5, Dis2D);
Matrix DenoiseFilter::Relation(5, 5);
Matrix DenoiseFilter::DepthROI(5, 5);
Matrix DenoiseFilter::AmpMatROI(5, 5);
Matrix DenoiseFilter::AmpRatio;

DenoiseFilter::DenoiseFilter()
{
	mInited = false;
}

DenoiseFilter::~DenoiseFilter()
{
}

void DenoiseFilter::Init(int w, int h)
{
	if(mInited == false)
		AmpMat.SetMatrix(h, w, NULL);
	mInited = true;
}

void DenoiseFilter::Denoise(int w, int h, unsigned short *phase, unsigned short *amplitude, unsigned char *flags,
							unsigned short *new_phase_frame, int amplitude_th, int32_t filter_level)
{
	int min_filter_dis = 500;
	int min_filter_num = 3;

	switch (filter_level)
	{
	case 1:
	{
		min_filter_dis = 500;
		min_filter_num = 18;
	}
	break;
	case 2:
	{
		min_filter_dis = 250;
		min_filter_num = 13;
	}
	break;
	case 3:
	{
		min_filter_dis = 100;
		min_filter_num = 8;
	}
	break;
	case 4:
	{
		min_filter_dis = 60;
		min_filter_num = 5;
	}
	break;
	default:
	{
		min_filter_dis = 500;
		min_filter_num = 18;
	}
	break;
	}

	Matrix depthMat(h, w, phase);
	memset(new_phase_frame, 0, (sizeof(unsigned short) * w * h));

	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			int Index = i*w + j;
			if (flags[Index] != 0)
			{
				AmpMat.data[i][j] = 0;
			}
			else if (amplitude[Index] > 300)
			{
				AmpMat.data[i][j] = 10;
			}
			else if (amplitude[Index]  > 200)
			{
				AmpMat.data[i][j] = 6;
			}
			else if (amplitude[Index]  > 100)
			{
				AmpMat.data[i][j] = 3;
			}
			else if (amplitude[Index]  > amplitude_th)
			{
				AmpMat.data[i][j] = 1;
			}
			else
			{
				AmpMat.data[i][j] = 0;
			}
		}
	}

	for (int i = 2; i < h - 2; i++)
	{

		for (int j = 2; j < w - 2; j++)
		{
			int Index = i*w + j;

			if (AmpMat.data[i][j] == 0)
			{
				new_phase_frame[Index] = 0;
				continue;
			}


			if (AmpMat.data[i][j] > 5)
			{
				AmpRatio = SmaMat;
			}
			else
			{
				AmpRatio = BigMat;
			}

			ROI55(&AmpMat, &AmpMatROI, j - 2, i - 2, 5, 5);

			ROI55(&depthMat, &DepthROI, j - 2, i - 2, 5, 5);

			float Center = DepthROI.data[2][2];
			int edgeNum = 0;
			for (int k = 0; k < 8; k++)
			{
				{
					if (Center - DepthROI.data[2 + dx1[k]][2 + dy1[k]]>min_filter_dis)
					{
						edgeNum++;
						Relation.data[2 + dx1[k]][2 + dy1[k]] = 0;
					}
					else if (Center - DepthROI.data[2 + dx1[k]][2 + dy1[k]] < -min_filter_dis)
					{
						edgeNum++;
						Relation.data[2 + dx1[k]][2 + dy1[k]] = 0;
					}
					else
					{
						Relation.data[2 + dx1[k]][2 + dy1[k]] = 1;
					}
				}
			}
			if ((edgeNum > min_filter_num))
			{
				new_phase_frame[Index] = 0;
				continue;
			}
			new_phase_frame[Index] = Convolution(&AmpMatROI, &Relation, &Dis2DMat, &DepthROI);
		}
	}
}
