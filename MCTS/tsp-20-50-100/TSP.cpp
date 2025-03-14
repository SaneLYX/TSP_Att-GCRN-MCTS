#include "include/TSP_IO.h"
#include "include/TSP_Basic_Functions.h"
#include "include/TSP_Init.h"
#include "include/TSP_2Opt.h"
#include "include/TSP_MCTS.h"
#include "include/TSP_Markov_Decision.h"
#include "Extra.h"
#include "kernel.h"
#include <pthread.h>
#include <cuda.h>

// For TSP20-50-100 instances
void Solve_One_Instance(int Inst_Index)
{
	
	Current_Instance_Best_Distance = Inf_Cost;

	// Input
	//cout<<"Step1"<<endl;
	Fetch_Stored_Instance_Info(Inst_Index);
	
	//Pre-processing	
	//cout<<"Step2"<<endl;
	Calculate_All_Pair_Distance();

	//cout<<"Step3"<<endl;
	Set_Heapmap_Fine_Name(Inst_Index);  	 	
  	Read_Heatmap(); 
	
	Identify_Candidate_Set();
	
	//cout<<"Step4"<<endl;
	
	Markov_Decision_Process_GPU();


	//Search by MDP  	 		  		    
	//Markov_Decision_Process();
	//cout<<"Step5"<<endl;
	double Stored_Solution_Double_Distance = Get_Stored_Solution_Double_Distance(Inst_Index);
	
	//cout<<"Step6"<<endl;
	//double Current_Solution_Double_Distance = Get_Current_Solution_Double_Distance();
	double Current_Solution_Double_Distance = Get_Current_Best_Double_Distance();

	if (Stored_Solution_Double_Distance / Magnify_Rate - Current_Solution_Double_Distance / Magnify_Rate > 0.000001)
		Beat_Best_Known_Times++;
	else if (Current_Solution_Double_Distance / Magnify_Rate - Stored_Solution_Double_Distance / Magnify_Rate > 0.000001)
		Miss_Best_Known_Times++;
	else
		Match_Best_Known_Times++;

	Sum_Opt_Distance += Stored_Solution_Double_Distance / Magnify_Rate;
	Sum_My_Distance += Current_Solution_Double_Distance / Magnify_Rate;
	Sum_Gap += (Current_Solution_Double_Distance - Stored_Solution_Double_Distance) / Stored_Solution_Double_Distance;

	printf("\nInst_Index:%d Concorde Distance:%f, MCTS Distance:%f Improve:%f Time:%.2f Seconds\n", Inst_Index + 1,
			Stored_Solution_Double_Distance / Magnify_Rate,
			Current_Solution_Double_Distance / Magnify_Rate, Stored_Solution_Double_Distance / Magnify_Rate - Current_Solution_Double_Distance / Magnify_Rate,
			((double)clock() - Current_Instance_Begin_Time) / CLOCKS_PER_SEC);

	FILE *fp;
	fp = fopen(Statistics_File_Name, "a+");
	fprintf(fp, "\nInst_Index:%d \t City_Num:%d \t Concorde:%f \t MCTS:%f Improve:%f \t Time:%.2f Seconds\n", Inst_Index + 1,
			Virtual_City_Num, Stored_Solution_Double_Distance / 1000000,
			Current_Solution_Double_Distance / Magnify_Rate, Stored_Solution_Double_Distance / Magnify_Rate - Current_Solution_Double_Distance / Magnify_Rate,
			((double)clock() - Current_Instance_Begin_Time) / CLOCKS_PER_SEC);

	fprintf(fp, "Solution: ");
	int Cur_City = Start_City;
	do
	{
		fprintf(fp, "%d ", Cur_City + 1);
		Cur_City = All_Node[Cur_City].Next_City;
	} while (Cur_City != Null && Cur_City != Start_City);

	fprintf(fp, "\n");
	fclose(fp);

	/*system("pause");
	exit(1);*/
	//Release_Memory(Virtual_City_Num);
	Free_H_Mem();
	//cout<<"Free_H_Mem"<<endl;
}

bool Solve_Instances_In_Batch()
{
	ifstream FIC;
	FIC.open(Input_File_Name);

	if (FIC.fail())
	{
		cout << "\n\nError! Fail to open file" << Input_File_Name << endl;
		getchar();
		return false;
	}
	else
		cout << "\n\nBegin to read instances information from " << Input_File_Name << endl;


	double Temp_X;
	double Temp_Y;
	int Temp_City;
	char Temp_String[100];

	for (int i = 0; i<Total_Instance_Num; i++)
	{
		for (int j = 0; j<Temp_City_Num; j++)
		{
			FIC >> Temp_X;
			FIC >> Temp_Y;
			Stored_Coordinates_X[i][j] = Temp_X;
			Stored_Coordinates_Y[i][j] = Temp_Y;
		}

		FIC >> &Temp_String[0];

		for (int j = 0; j<Temp_City_Num; j++)
		{
			FIC >> Temp_City;
			Stored_Opt_Solution[i][j] = Temp_City - 1;
		}

		FIC >> Temp_City;
	}
	FIC.close();

	cout << "\nRead instances finished. Begin to search." << endl;

	if ((Index_In_Batch + 1)*Inst_Num_Per_Batch < Total_Instance_Num)
		Test_Inst_Num = Inst_Num_Per_Batch;
	else
		Test_Inst_Num = Total_Instance_Num - Index_In_Batch*Inst_Num_Per_Batch;
	cout << "\nNumber of instances in current batch: " << Test_Inst_Num << endl;

	FILE *fp;
	fp = fopen(Statistics_File_Name, "w+");
	
	fprintf(fp, "Number_of_Instances_In_Current_Batch: %d\n", Test_Inst_Num);
	fclose(fp);
	
	//cout<<"FP"<<endl;
	for (int i = Index_In_Batch*Inst_Num_Per_Batch; i<(Index_In_Batch + 1)*Inst_Num_Per_Batch && i<Total_Instance_Num; i++)
		{
			Solve_One_Instance(i);
			//printf("Instance-%d is solved.\n", i);
		}

	return true;
}

int main(int argc, char ** argv)
{
	double Overall_Begin_Time = (double)clock();
	
	//srand(Random_Seed);
	srand(time(NULL));
	int gpu;
	for (int i = 0; i < 1; i++)
	{
		Index_In_Batch = atoi(argv[1]); //i;
		Statistics_File_Name = argv[2]; //"../results/result_20.txt";
		Input_File_Name = argv[3]; //"../instances/tsp20_test_concorde.txt";
		Temp_City_Num = atoi(argv[4]); //20;
		gpu = atoi(argv[5]);
	}
	cudaSetDevice(gpu);
	Solve_Instances_In_Batch();

	FILE *fp;
	fp = fopen(Statistics_File_Name, "a+");
	fprintf(fp, "\n\nAvg_Concorde_Distance: %f Avg_MCTS_Distance: %f Avg_Gap: %f Total_Time: %.2f Seconds \n Beat_Best_Known_Times: %d Match_Best_Known_Times: %d Miss_Best_Known_Times: %d \n",
		Sum_Opt_Distance / Test_Inst_Num, Sum_My_Distance / Test_Inst_Num, Sum_Gap / Test_Inst_Num, ((double)clock() - Overall_Begin_Time) / CLOCKS_PER_SEC, Beat_Best_Known_Times, Match_Best_Known_Times, Miss_Best_Known_Times);
	fclose(fp);

	printf("\n\nAvg_Concorde_Distance: %f Avg_MCTS_Distance: %f Avg_Gap: %f Total_Time: %.2f Seconds \n Beat_Best_Known_Times: %d Match_Best_Known_Times: %d Miss_Best_Known_Times: %d \n",
		Sum_Opt_Distance / Test_Inst_Num, Sum_My_Distance / Test_Inst_Num, Sum_Gap / Test_Inst_Num, ((double)clock() - Overall_Begin_Time) / CLOCKS_PER_SEC, Beat_Best_Known_Times, Match_Best_Known_Times, Miss_Best_Known_Times);
	getchar();

	return 0;
}
