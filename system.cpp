#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <string>
#include <conio.h>


#include "molecule.h"
#include "method.h"
#include "geometry.h"
#include "system.h"



using namespace std;


System::System(string _source) {
	source = _source;
}

void System::ReadParameters() {
	
	ifstream ifs(source, ifstream::in);
	string word;
	while (ifs.good()) {
		ifs >> word;
		if (word.find("geometry") != string::npos) ifs >> geometry_name;

		if (word.find("layers_x") != string::npos) ifs >> layers_x;
		if (word.find("layers_y") != string::npos) ifs >> layers_y;
		if (word.find("curvature") != string::npos) ifs >> Rcurv;
	}
	ifs.close();
}

void System::SetGeometry() {

	if (geometry_name == "polar") {
		Polar polar;
		polar.GetValue(layers_x, layers_y, Rcurv);
		polar.AllocateMemory();
		polar.UpdateVolume();
		polar.UpdateSquareFront();
		polar.UpdateSquareUp();
		polar.UpdateSquareSide();
		polar.Transposition();
		
		geo = polar;
	}
	else if (geometry_name == "torus") {
		Torus torus;
		torus.GetValue(layers_x, layers_y, Rcurv);
		torus.AllocateMemory();
		torus.UpdateVolume();
		torus.UpdateSquareFront();
		torus.UpdateSquareUp();
		torus.UpdateSquareLeft();
		torus.UpdateSquareRight();
		torus.Transposition();

		geo = torus;
	}
	else {
		printf("ERROR: unknown geometry\n");
		return;
	}
	
	//geo.PrintVolume();

}

void System::ReadMolecules() {
	
	ifstream ifs(source, ifstream::in);
	string word;
	while (ifs.good()) {
		ifs >> word;
		if (word.find("molecule") != string::npos) {
			Molecule new_mol;
			while (word != "[" && ifs.good()) {
				ifs >> word;
				if (word.find("Ns") != string::npos) ifs >> new_mol.ns;
				if (word.find("gen") != string::npos) ifs >> new_mol.num_generation;
				if (word.find("sigma") != string::npos) ifs >> new_mol.sigma;
				if (word.find("chi") != string::npos) ifs >> new_mol.chi;
				if (word.find("xmin") != string::npos) ifs >> new_mol.xmin;
				if (word.find("xmax") != string::npos) ifs >> new_mol.xmax;
				if (word.find("ymin") != string::npos) ifs >> new_mol.ymin;
				if (word.find("ymax") != string::npos) ifs >> new_mol.ymax;

			}

			new_mol.MyNewMethod();
			mol.push_back(new_mol);
		}
	}
	ifs.close();

	
	for (int i = 0; i < mol.size(); i++) {
		cout << "number of molecule: " << i + 1 << endl;
		cout << mol[i].ns << endl;
		cout << mol[i].num_generation << endl;
		cout << mol[i].xmin << endl;

	}
};

void System::ReadMethods() {

	ifstream ifs(source, ifstream::in);
	string word;
	while (ifs.good()) {
		ifs >> word;
		if (word.find("method") != string::npos) {
			double tolerance_, nu_, _num_iter;
			string name_method;
			// ���� �� �������� ����� ���� ��� �� ���������� ����
			while (word != "[" && ifs.good()) {
				ifs >> word;
				if (word.find("type") != string::npos) ifs >> name_method;
				if (word.find("tolerance") != string::npos) ifs >> tolerance_;
				if (word.find("num_iter") != string::npos) ifs >> _num_iter;
				if (word.find("step") != string::npos) ifs >> nu_;
			}
			//num_iter_, tolerance_, step_
			if (name_method == "gradient") {
				methods.push_back(make_unique<Gradient>(_num_iter, tolerance_, nu_));
			}
			else if (name_method == "DFP") {
				methods.push_back(make_unique<DFP>(_num_iter, tolerance_, nu_));
			}
			else {
				printf("ERROR: unknown method");
			}

		}
	}
	ifs.close();

	/*for (BaseOptimTools &w : methods) {
		cout << w.name << endl;
		cout << w.tolerance << endl;
		cout << w.nu << endl;
		cout << w.num_iter << endl;
	}*/
}

void System::Function() {


	for (int t = 0; t < mol.size(); ++t) {


		mol[t].FindG();

		for (int i = mol[t].xmin; i < mol[t].xmax + 1; ++i) {
			for (int k = mol[t].ymin; k < mol[t].ymax + 1; ++k) {
				mol[t].Gforw[i][k][0] = mol[t].G[i][k];
			}
		}

		mol[t].FindGforw();

		for (int i = 1; i < layers_x + 1; ++i) {
			for (int k = 1; k < layers_y + 1; ++k) {
				mol[t].Gback[i][k][mol[t].ns - 1] = mol[t].G[i][k];
			}
		}

		mol[t].FindGback();
		mol[t].FindQ();


		FindFiP(t);
		FindFiSide(t);

		FindLagrangeMultipliers(t);
	}

	FindFiSolv();
	FindFiTotal();
	FindLagrangeMultipliers();
	FindGrad();

	
};


void System::FindFiSide(int t) {

	

		for (int i = -1; i <= 1; ++i) {
			for (int j = -1; j <= 1; ++j) {
				if (i == j == -1) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_bb[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == -1 && j == 0) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_bn[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == -1 && j == 1) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_bf[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == 0 && j == -1) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_nb[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == 0 && j == 0) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_nn[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == 0 && j == 1) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_nf[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == 1 && j == -1) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_fb[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == 1 && j == 0) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_fn[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
				else if (i == 1 && j == 1) {
					for (int l = 0; l < layers_x + 1; ++l) {
						for (int k = 0; k < layers_y + 1; ++k)
							mol[t].fi_side[l * (layers_y + 2) + k] += geo.lambda_ff[l][k] * mol[t].fi_p[l+i][k+j];
					}
				}
			}

		}
}

void System::FindFiP(int t) {
	double sum;
	for (int i = 1; i < layers_x + 1; ++i) {
		for (int k = 1; k < layers_y + 1; ++k) {
			sum = 0;
			for (int j = 0; j < mol[t].ns; ++j)
				sum += (mol[t].Gforw[i][k][j] * mol[t].Gback[i][k][j]) / mol[t].G[i][k];

			mol[t].fi_p[i][k] = (mol[t].theta / mol[t].q)*sum;
		}
	}
};


void System::FindFiSolv() {

	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			fi_solv[i][j]= exp(-u[i*(layers_y + 2) + j]);
	}

}

void System::FindFiTotal() {

	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			fi_total[i][j] = mol[0].fi_p[i][j] + mol[1].fi_p[i][j] + fi_solv[i][j];
	}
}


void System::FindLagrangeMultipliers(int t) {

	vector <vector <double>> c(layers_x + 2, vector<double>(layers_y + 2,0));


	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			mol[t].fi_p[i][j] = mol[t].fi_p[i][j] * chi_seg;
	}

	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			fi_solv[i][j] = (fi_solv[i][j] - 1)*mol[t].chi;
	}

	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			c[i][j]= fi_solv[i][j] +  mol[t].fi_p[i][j];
	}


	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			mol[t].multipliers[i][j] = mol[t].u[i*(layers_y + 2) + j] - c[i][j]/fi_total[i][j]; ///?????
	}

};


void System::FindLagrangeMultipliers() {


	vector <vector <double>> c(layers_x + 2, vector<double>(layers_y + 2, 0));


	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			c[i][j] = mol[0].fi_p[i][j] * mol[0].chi + mol[1].fi_p[i][j] * mol[1].chi;
	}


	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			multipliers[i][j] = u[i*(layers_y + 2) + j] - c[i][j] / fi_total[i][j]; ///?????
	}


};


void System::FindGrad() {

	for (int i = 1; i < layers_x + 1; ++i) {
		for (int j = 1; j < layers_y + 1; ++j)
			middle_multipliers[i][j] = 1/3*(mol[0].multipliers[i][j] + mol[1].multipliers[i][j] + multipliers[i][j]);
	}

	for (int z = 0; z < M; ++z) {
		for (int i = 1; i < layers_x + 1; ++i) {
			for (int j = 1; j < layers_y + 1; ++j)
				grad[i*(layers_y + 2) + j] = 1 - 1 / fi_total[i][j] + middle_multipliers[i][j] + mol[0].multipliers[i][j];
		}
	}

	for (int z = M; z < 2*M; ++z) {
		for (int i = 1; i < layers_x + 1; ++i) {
			for (int j = 1; j < layers_y + 1; ++j)
				grad[i*(layers_y + 2) + j] = 1 - 1 / fi_total[i][j] + middle_multipliers[i][j] + mol[1].multipliers[i][j];
		}
	}


	for (int z = 2*M; z < 3*M ; ++z) {
		for (int i = 1; i < layers_x + 1; ++i) {
			for (int j = 1; j < layers_y + 1; ++j)
				grad[i*(layers_y + 2) + j] = 1 - 1 / fi_total[i][j] + middle_multipliers[i][j] + multipliers[i][j];
		}
	}

};


void System::AllocateMemory() {

	u.resize(M + 2);
	grad.resize(M + 2);
	fi_solv.assign(layers_x + 1, vector<double>(layers_y + 1, 0));
	fi_total.assign(layers_x + 1, vector<double>(layers_y + 1, 0));
	multipliers.assign(layers_x + 1, vector<double>(layers_y + 1, 0));
	middle_multipliers.assign;

};

void System::Cycling() {

	int M = 3 * (layers_x + 2)*(layers_y + 2);

	for (BaseOptimTools &scheme : methods) {
		cout << scheme.name << endl;

		
		for (int i = 0; i < mol.size(); ++i) {
			mol[i].AllocateMemory();
		}

		AllocateMemory();

		scheme.SetParameters(layers_x, layers_y, grad, u);

		double length_of_grad = 0.0;
		int step = 0.0;

		Function();
		length_of_grad = scheme.SetGradFirst(grad);

		do {
			scheme.UpdateX(u, grad);
			Function();
			length_of_grad = scheme.SetGradRegular(grad);

			step++;
			cout << step << "     " << length_of_grad << endl;
		} while ((length_of_grad > scheme.tolerance) && (step < scheme.num_iter));
	}
};



void System::Output() {

	FILE * txt = fopen("fi_p_out.txt", "w");
	fprintf(txt, "%5d ", (int)layers_y);
	for (int j = 1; j < layers_y + 1; j++) {
		fprintf(txt, " %11d", j);
	}
	fprintf(txt, "\n");
	for (int i = 1; i < layers_x + 1; i++) {
		fprintf(txt, "%5d ", i);
		for (int j = 1; j < layers_y + 1; j++) {
			fprintf(txt, " %8.5e", fi_total[i][j]);
		}
		fprintf(txt, "\n");
	}
	fclose(txt);

};