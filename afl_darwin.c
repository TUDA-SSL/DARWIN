#include "afl_darwin.h"
#include "rand.h"
#include <stdio.h>

unsigned kAflNumMutationOperators;

#ifdef REAL_VALUED
	#define VALUE_TYPE double
#else
	#define VALUE_TYPE bool
#endif

#ifndef _nParents
#define _nParents 5u
#endif

#ifndef _lambda
#define _lambda 4u
#endif

unsigned int *_nextToEvaluate;  // next child to evaluate
unsigned int *_currentParent;   // current parent being mutated
unsigned int *_bestSoFar;       // best so far parent
unsigned int *_bestSoFarChild;  // best so far child (for a single parent)
int **_parentsFitness;
int **_childrenFitness;

VALUE_TYPE ***_parents;
VALUE_TYPE ***_children;
VALUE_TYPE **_currentRelativeProb;

/*
 * Initialize data structures for DARWIN
 * @nr_seeds: number of different initial seeds
 * @nr_mutations: number of mutation operators
*/
void DARWIN_init(uint64_t nr_seeds, unsigned nr_mutations) {
	// init RNG
	rand_init();

	kAflNumMutationOperators = nr_mutations;

	// initialize opt alg data structures
	_nextToEvaluate = (unsigned int *) malloc(nr_seeds * sizeof(unsigned int));
	_currentParent = (unsigned int *) malloc(nr_seeds * sizeof(unsigned int));
	_bestSoFar = (unsigned int *) malloc(nr_seeds * sizeof(unsigned int));         // best so far parent
	_bestSoFarChild = (unsigned int *) malloc(nr_seeds * sizeof(unsigned int));    // best so far child (for a single parent)
	_parentsFitness = (int **) malloc(nr_seeds * sizeof(int *));
	_childrenFitness = (int **) malloc(nr_seeds * sizeof(int *));

	_parents = (VALUE_TYPE ***) malloc(nr_seeds * sizeof(VALUE_TYPE **));
	_children = (VALUE_TYPE ***) malloc(nr_seeds * sizeof(VALUE_TYPE **));
	
	_currentRelativeProb = (VALUE_TYPE **) malloc(nr_seeds * sizeof(VALUE_TYPE *));

	printf("nParents: %u, lambda %u\n", _nParents, _lambda);

	for (int seed = 0; seed < nr_seeds; seed++) {
		_nextToEvaluate[seed] = 0;
		_bestSoFar[seed] = 0;
		_bestSoFarChild[seed] = 0;
		_currentParent[seed] = 0;

		_parentsFitness[seed] = malloc(_nParents * sizeof(int));
		_childrenFitness[seed] = malloc(_lambda * sizeof(int));

		_children[seed] = (VALUE_TYPE **) malloc(_lambda * sizeof(VALUE_TYPE *));
		for (int i = 0; i < _lambda; i++)
			_children[seed][i] = malloc(kAflNumMutationOperators * sizeof(VALUE_TYPE));

		_parents[seed] = (VALUE_TYPE **) malloc(_nParents * sizeof(VALUE_TYPE *));
		for (int i = 0; i < _nParents; i++)
			_parents[seed][i] = malloc(kAflNumMutationOperators * sizeof(VALUE_TYPE));

		memset(_parentsFitness[seed], 0, _nParents);
		memset(_childrenFitness[seed], 0, _lambda);

		// initial random values for the parents and first child individual
		for(uint i = 0; i < _nParents; i++) {
			for(uint j = 0; j < kAflNumMutationOperators; j++) {
				// TODO: optionally replace with an alternate randomizer
				_parents[seed][i][j] = rand() > (RAND_MAX / 2);
			}
		}

		for(uint i = 0; i < kAflNumMutationOperators; i++) {
			_children[seed][_nextToEvaluate[seed]][i] = rand() > (RAND_MAX / 2);
		}

		_currentRelativeProb[seed] = _children[seed][_nextToEvaluate[seed]];
	}
}

/*
 * Choose an AFL mutation operator
 * @seed: seed to select per-seed vector
*/
int DARWIN_SelectOperator(uint64_t seed)
{
#ifdef REAL_VALUED
	double *v = _currentRelativeProb[seed];
	uint v_size = kAflNumMutationOperators;
	double cumulative[kAflNumMutationOperators];
	for (uint i = 0; i < kAflNumMutationOperators; i++)	
		cumulative[i] = 0.0;

	double minVal = 0.0;
	double maxVal = 1.0;
	for (uint i = 0; i < kAflNumMutationOperators; i++) {
		if (v[kAflNumMutationOperators] < minVal) {
			minVal = v[kAflNumMutationOperators];
		} else {
			if (v[kAflNumMutationOperators] > maxVal) {
				maxVal = v[kAflNumMutationOperators];
			}
		}
	}

	cumulative[0] = 1 + (kAflNumMutationOperators - 1) * (v[0] - minVal) / (maxVal - minVal); // selection pressure is kAflNumMutationOperators
	for(uint i = 1; i < kAflNumMutationOperators; i++) {
		cumulative[i] = 1 + (kAflNumMutationOperators - 1) * (v[i] - minVal)/(maxVal - minVal);
		cumulative[i] += cumulative[i - 1];
	}

	double rngNormVal = rand_32_double();

	double randVal = rngNormVal * cumulative[kAflNumMutationOperators - 1];

	int chosen = 0;
	// didn't bother with binary search since negligible compared to other modules
	while(cumulative[chosen] < randVal)
		chosen++;
	return chosen;
#else
	// baseline:
	bool *v = (bool *) _currentRelativeProb[seed];

	// select a random mutation operator with flag == true
	// revert to random if all false
	uint operatorId = rand_32_int(kAflNumMutationOperators);
	uint nTries = 0;
	while (v[operatorId] == false && nTries < kAflNumMutationOperators) {
		nTries++;
		operatorId = (operatorId + 1) % kAflNumMutationOperators;
	}
    	return operatorId;
#endif
}


/*
 * Report feedback to DARWIN
 * @seed: seed to attribute this to
 * @numPaths: number of new paths
*/
void DARWIN_NotifyFeedback(uint64_t seed, unsigned numPaths)
{
	// update this candidate solution fitness
	_childrenFitness[seed][_nextToEvaluate[seed]] = numPaths;

	// update if new best child found
	if(_childrenFitness[seed][_nextToEvaluate[seed]] > _childrenFitness[seed][_bestSoFarChild[seed]]) {
		_bestSoFarChild[seed] = _nextToEvaluate[seed];
	}
	// move to next child candidate
	_nextToEvaluate[seed]++;
	
	// if all children evaluated
	if(_nextToEvaluate[seed] == _lambda) {
		// set best child as future parent (only if better than the parent)
		if(_childrenFitness[seed][_bestSoFarChild[seed]] > _parentsFitness[seed][_currentParent[seed]]) {
			for(uint i = 0; i < kAflNumMutationOperators; i++) {
				_parents[seed][_currentParent[seed]][i] = _children[seed][_bestSoFarChild[seed]][i];
			}

			_parentsFitness[seed][_currentParent[seed]] = _childrenFitness[seed][_bestSoFarChild[seed]];
		}
		// update best parent solution if needed
		if(_parentsFitness[seed][_currentParent[seed]] > _parentsFitness[seed][_bestSoFar[seed]]) {
			_bestSoFar[seed] = _currentParent[seed];
		}

		// move to next parent (or return to first)
		_currentParent[seed] = (_currentParent[seed] + 1) % _nParents;

		// reset indices
		_bestSoFarChild[seed] = 0;
		_nextToEvaluate[seed] = 0;

		// reset children scores
		memset(_childrenFitness[seed], 0, _lambda); 
	}
	
	// if there are children to evaluate, generate new candidate and return
	if(_nextToEvaluate[seed] < _lambda) {
		_currentRelativeProb[seed] = &(_children[seed][_nextToEvaluate[seed]][0]);

		for(uint i = 1; i < kAflNumMutationOperators; i++) {
			_currentRelativeProb[seed][i] = _parents[seed][_currentParent[seed]][i];
		}
		
		// select a single random gene and invert
		int randomGene = rand_32_int(kAflNumMutationOperators);
#ifdef REAL_VALUED
		double mutation = (rand_32_double_gauss() - 0.5) / 4;
		_currentRelativeProb[seed][randomGene] += mutation;
		if(_currentRelativeProb[seed][randomGene] < 0)
			_currentRelativeProb[seed][randomGene] = 0;
#else		
		_currentRelativeProb[seed][randomGene] = !_currentRelativeProb[seed][randomGene];
#endif
	}
}

/*
 * Get the best parent solution so far as a vector (hardcoded to max mutation operators in AFL)
 * @seed: seed to attribute this to
*/
uint32_t DARWIN_get_parent_repr(uint64_t seed) {
	uint32_t value;
	bool *_currentParentBool = (bool *) (_parents[seed][_bestSoFar[seed]]); 
	for (int i = 0; i < 15; i++) {
		printf("%d: %s\n", i,_currentParentBool[i] ? "true" : "false");
	}
	/* encode parent in a number */
	for (int i = 14; i >= 0; i--) {
		value |= (_parents[seed][_bestSoFar[seed]][i]) ? (1 << i) : 0;
	}
	printf("val: %d\n", value);
	return value;
}
