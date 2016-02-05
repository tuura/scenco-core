#include "includes_defines.h"
#include "types.h"
#include "global.h"
#include "utilities.cpp"
#include "load_scenarios.cpp"
#include "heuristic_function.cpp"
#include "custom_encoding.cpp"
#include "encoding_methods.cpp"

extern "C" {

	int get_bit(int row, int col){
		return ((int)opcodes[row][col]);
	}

	int get_opcodes_length(){
		return bits;
	}

	int load_graphs_opcodes(char *file_in,
			char *custom_file_name){

		FILE *fp;
		int trivial = 0;
		int err=0;
		int elements;
		int min_disp;
		int len_sequence = 0;

		if(temporary_files_creation() != 0){
			return -1;
		}
		if( (fpLOG = fopen(LOG,"w")) == NULL){
			fprintf(stderr,"Error on opening LOG file for writing.\n");
		}
		fprintf(fpLOG,WELCOME_STRING);	

		// memory allocation
		fprintf(fpLOG,"Allocating memory for vertex names and graphs...");
		g = (GRAPH_TYPE *) malloc(sizeof(GRAPH_TYPE) * scenariosLimit);
		fprintf(fpLOG,"DONE\n");
		fflush(stdout);

		//**********************************************************************
		// Building CPOG Part
		//**********************************************************************

		// loading scenarios
		fprintf(fpLOG,"\nOptimal scenarios encoding and CPOG synthesis.\n");	
		if(loadScenarios(file_in, fp) != 0){
			fprintf(stderr,"Loading scenarios failed.\n");
			return -1;
		}
		fprintf(fpLOG,"\n%d scenarios have been loaded.\n", n);
	
		// looking for predicates
		if(predicateSearch() != 0){
			fprintf(stderr,"Predicate searching failed.\n");
			return -1;
		}
	
		// looking for non-trivial constraints
		if( (fp = fopen(CONSTRAINTS_FILE,"w")) == NULL){
			fprintf(stderr,"Error on opening constraints file for writing.\n");
			return -1;
		}
		if(nonTrivialConstraints(fp, &total, &trivial) != 0){
			fprintf(stderr,"Non-trivial constraints searching failed.\n");
			return -1;
		}
		fprintf(fpLOG,"\n%d non-trivial encoding constraints found:\n\n", total - trivial);

		// writing non-trivial constraints into a file
		if( (fp = fopen(TRIVIAL_ENCODING_FILE,"w")) == NULL){
			fprintf(stderr,"Error on opening constraints file for writing.\n");
			return -1;
		}
		for(int i = 0; i < total; i++)
			if (!encodings[i].trivial) {
				fprintf(fp,"%s\n",encodings[i].constraint.c_str());
		}
		fclose(fp);
	
		fprintf(fpLOG,"\nBuilding conflict graph... ");
		if(conflictGraph(&total) != 0){
			fprintf(stderr,"Building conflict graph failed.\n");
			return -1;
		}
		fprintf(fpLOG,"DONE.\n");

		//**********************************************************************
		// Reading encoding set by the user
		//**********************************************************************

		fprintf(fpLOG,"Reading encodings set... ");
		fflush(stdout);
		if(read_set_encoding(custom_file_name,n,&bits) != 0){
			fprintf(stderr,"Error on reading encoding set.\n");
			removeTempFiles();
			return -1;
		}
		fprintf(fpLOG,"DONE\n");

		fprintf(fpLOG,"Check correcteness of encoding set... ");
		if(check_correctness(custom_file_name,n,tot_enc,bits) != 0){
			removeTempFiles();
			return -1;
		}
		fprintf(fpLOG,"DONE\n");

		//**********************************************************************
		// Variable preparation for encoding
		//**********************************************************************
		strcpy(file_in,TRIVIAL_ENCODING_FILE);
		file_cons = strdup(CONSTRAINTS_FILE);

		/*READ NON-TRIVIAL ENCODING FILE*/
		fprintf(fpLOG,"Reading non-trivial encoding file... ");
		if( (err = read_file(file_in, &cpog_count, &len_sequence)) ){
			fprintf(stderr,"Error occured while reading non-trivial encoding file, error code: %d", err);
			removeTempFiles();
			return -1;
		}
		fprintf(fpLOG,"DONE\n");

		/*SEED FOR RAND*/
		srand(time(NULL));

		/*ALLOCATING AND ZEROING DIFFERENCE MATRIX*/
		opt_diff = (int**) calloc(n, sizeof(int*));
		for(int i=0;i<n;i++)
			opt_diff[i] = (int*) calloc(n, sizeof(int));

		/*NUMBER OF POSSIBLE ENCODING*/
		tot_enc = 1;
		for(int i=0;i<bits;i++) tot_enc *= 2;

		/*ANALYSIS IF IT'S A PERMUTATION OR A DISPOSITION*/
		num_perm = 1;
		if (n == tot_enc){
			/*PERMUTATION*/
			if(!unfix && !SET){
				for(int i = 1; i< tot_enc; i++)
					num_perm *= i;
			}else{
				for(int i = 1; i<= tot_enc; i++)
					num_perm *= i;
			}
			fprintf(fpLOG,"Number of possible permutations by fixing first element: %lld\n", num_perm);
		}
		else{
			/*DISPOSITION*/
			if(!unfix && !SET){
				elements = tot_enc-1;
				min_disp = elements - (n- 1) + 1;
			}else{
				elements = tot_enc;
				min_disp = elements - (n) + 1;
			}
				num_perm = 1;
			for(int i=elements; i>= min_disp; i--)
				num_perm *= i;
			fprintf(fpLOG,"Number of possible dispositions by fixing first element: %lld\n", num_perm);
		}

		all_perm = num_perm;
		num_perm = EXHAUSTIVE_MAX_PERM;

		/*PREPARATION DATA FOR ENCODING PERMUTATIONS*/
		enc = (int*) calloc(tot_enc, sizeof(int));

		/*First element is fixed*/
		if (!unfix && !SET)
			enc[0] = 1;
	
		sol = (int*) calloc(tot_enc, sizeof(int));
		if (sol == NULL){
			fprintf(stderr,"solution variable = null\n");
			removeTempFiles();
			return -1;
		}
		perm = (int**) malloc(sizeof(int*) * num_perm);
		if ( perm == NULL){
			fprintf(stderr,"perm variable = null\n");
			removeTempFiles();
			return -1;
		}
		for(long long int i=0;i<num_perm;i++){
			perm[i] = (int*) malloc(n * sizeof(int));
			if (perm[i] == NULL){
				fprintf(stderr,"perm[%lld] = null\n",i);
				removeTempFiles();
				return 3;
			}
		}
		weights = (float*) calloc(num_perm, sizeof(float));

		/*BUILDING DIFFERENCE MATRIX*/
		fprintf(fpLOG,"Building DM (=Difference Matrix)... ");
		if( (err = difference_matrix(n, len_sequence)) ){
			fprintf(stderr,"Error occurred while building difference matrix, error code: %d", err);
			removeTempFiles();
			return 3;
		}
		fprintf(fpLOG,"DONE\n");

		fclose(fpLOG);
		removeTempFiles();

		return 0;
	}

	int single_literal_encoding(){

		if( (fpLOG = fopen(LOG,"a")) == NULL){
			fprintf(stderr,"Error on opening LOG file for appending.\n");
		}

		fprintf(fpLOG,"Running single-literal encoding.\n");

		if(singleLiteralEncoding(total) != 0){
			fprintf(stderr,"Single-literal encoding failed.\n");
			return -1;
		}

		if (export_variables(single_literal) != 0){
			return -1;
		}

		fclose(fpLOG);

		return 0;
	}

	int sequential_encoding(){

		if( (fpLOG = fopen(LOG,"a")) == NULL){
			fprintf(stderr,"Error on opening LOG file for appending.\n");
		}

		fprintf(fpLOG,"Running Sequential encoding.\n");

		if(sequentialEncoding() != 0){
			fprintf(stderr,"Sequential encoding failed.\n");
			return -1;
		}

		if (export_variables(sequential) != 0){
			return -1;
		}

		fclose(fpLOG);

		return 0;
	}

	int random_encoding(){

		if( (fpLOG = fopen(LOG,"a")) == NULL){
			fprintf(stderr,"Error on opening LOG file for appending.\n");
		}

		fprintf(fpLOG,"Running Random encoding.\n");
		
		num_perm = 1;
		if(randomEncoding() != 0){
			fprintf(stderr,"Random encoding failed.\n");
			return -1;
		}

		if (export_variables(randomE) != 0){
			return -1;
		}

		fclose(fpLOG);

		return 0;
	}

	int heuristic_encoding(){

		if( (fpLOG = fopen(LOG,"a")) == NULL){
			fprintf(stderr,"Error on opening LOG file for appending.\n");
		}

		fprintf(fpLOG,"Running Random generation... ");
		num_perm = 1;
		if(randomEncoding() != 0){
			fprintf(stderr,"Random encoding failed.\n");
			return -1;
		}
		fprintf(fpLOG,"DONE.\n");

		fprintf(fpLOG,"Running Heuristic optimisation... ");
		if(start_simulated_annealing() != 0){
			fprintf(stderr,"Heuristic optimisation failed.\n");
			return -1;
		}
		fprintf(fpLOG,"DONE.\n");

		if (export_variables(heuristic) != 0){
			return -1;
		}

		fclose(fpLOG);
	}

	int exhaustive_encoding(){

		if( (fpLOG = fopen(LOG,"a")) == NULL){
			fprintf(stderr,"Error on opening LOG file for appending.\n");
		}

		if(all_perm > EXHAUSTIVE_MAX_PERM || all_perm < 0){
			fprintf(stderr,"Solution space is too wide to be inspected.\n");
			removeTempFiles();
			return -1;
		} else {
			num_perm = all_perm;
		}

		fprintf(fpLOG,"Running Exhaustive encoding.\n");

		if(!unfix && !SET){
			//permutation_stdalgo(cpog_count,tot_enc);
			fprintf(fpLOG,"Permutation algorithm unconstrained... ");
			exhaustiveEncoding(sol,0,enc,cpog_count, tot_enc);
			fprintf(fpLOG,"DONE\n");
		}else{
			fprintf(fpLOG,"Permutation algorithm constrained... ");
			exhaustiveEncoding(sol,-1,enc,cpog_count, tot_enc);
			fprintf(fpLOG,"DONE\n");

			fprintf(fpLOG,"Filtering encoding... ");
			filter_encodings(cpog_count, bits, tot_enc);
			fprintf(fpLOG,"DONE\n");
		}

		if (export_variables(exhaustive) != 0){
			return -1;
		}

		fclose(fpLOG);

		return 0;
	}
}
