//
// Created by Hood on 5/22/2019.
//

#include "FpVector.h"
#include "combinatorics.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int array_to_string(char* buffer, uint* A, uint length){
    buffer[0] = '[';
    buffer[1] = '\0';
    int len = 1;
    for (int i = 0; i < length; i++) {
        len += sprintf(buffer + len, "%d, ", A[i]);
    }
    len += sprintf(buffer + len, "]");
    return len;
}


typedef struct {
    VectorInterface * interface;
    uint p;
    uint dimension;
    uint size;
    uint offset;
    uint64* vector;
} VectorStd;


// Generated with Mathematica:
//     Ceiling[Log2[# (# - 1) + 1 &[Prime[Range[54]]]]]
// But for 2 it should be 1.
uint bitlengths[MAX_PRIME_INDEX] = { 
     1, 3, 5, 6, 7, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12, 12, 12, 12, 13,     
     13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15,    
     15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 
};

uint getBitlength(uint p){
    return bitlengths[prime_to_index_map[p]];
}

// Generated with Mathematica:
//     2^Ceiling[Log2[# (# - 1) + 1 &[Prime[Range[54]]]]]-1
// But for 2 it should be 1.
uint bitmasks[MAX_PRIME_INDEX] = {
    1, 7, 31, 63, 127, 255, 511, 511, 511, 1023, 1023, 2047, 2047, 2047, 
    4095, 4095, 4095, 4095, 8191, 8191, 8191, 8191, 8191, 8191, 16383, 
    16383, 16383, 16383, 16383, 16383, 16383, 32767, 32767, 32767, 32767, 
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535
};

uint getBitMask(uint p){
    return bitmasks[prime_to_index_map[p]];
}

// Generated with Mathematica:
//      Floor[64/Ceiling[Log2[# (# - 1) + 1 &[Prime[Range[54]]]]]]
// But for 2 it should be 64.
uint entries_per_64_bits[MAX_PRIME_INDEX] = {
    64, 21, 12, 10, 9, 8, 7, 7, 7, 6, 6, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4,  
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

uint getEntriesPer64Bits(uint p){
    return entries_per_64_bits[prime_to_index_map[p]];   
}

uint * modplookuptable[MAX_PRIME_INDEX] = {0};

void initializeModpLookupTable(uint p){
    uint p_times_p_minus_1 = p*(p-1);
    uint * table = malloc((p_times_p_minus_1 + 1) * sizeof(uint));
    for(uint i = 0; i <= p_times_p_minus_1; i++){
        table[i] = i % p;
    }
    modplookuptable[prime_to_index_map[p]] = table;
}

// n must be in the range 0 <= n <= p * (p-1)
uint modPLookup(uint p, uint n){
    return modplookuptable[prime_to_index_map[p]][n];
}

typedef struct {
    uint limb;
    uint bit_index;
} LimbBitIndexPair;

LimbBitIndexPair * limbBitIndexLookupTable[MAX_PRIME_INDEX] = {0};

void initializeLimbBitIndexLookupTable(uint p){
    uint p_idx = prime_to_index_map[p];
    uint entries_per_limb = getEntriesPer64Bits(p);
    uint bit_length = getBitlength(p);
    LimbBitIndexPair * table = malloc(MAX_DIMENSION * sizeof(LimbBitIndexPair));
    for(uint i = 0; i < MAX_DIMENSION; i++){
        table[i].limb = i/entries_per_limb;
        table[i].bit_index = (i % entries_per_limb) * bit_length;
    }
    limbBitIndexLookupTable[p_idx] = table;
}

LimbBitIndexPair getLimbBitIndexPair(uint p, uint idx){
    return limbBitIndexLookupTable[prime_to_index_map[p]][idx];
}

uint getVectorSize(uint p, uint dimension, uint offset){
    uint bit_length = getBitlength(p);
    uint size = (dimension == 0) ? 0 : (getLimbBitIndexPair(p, dimension + offset/bit_length - 1).limb + 1);
    return size; // Need extra space for vector fields.
}

Vector* initializeVector(VectorInterface *interface, uint p, uint64 *container, uint64 *memory, uint dimension, uint offset){
    uint bit_length = getBitlength(p);
    VectorStd * v = (VectorStd *) container;
    v->interface = interface;
    v->p = p;
    v->dimension = dimension;
    v->size = dimension == 0 ? 0 : getLimbBitIndexPair(p, dimension + offset/bit_length - 1).limb + 1;
    v->offset = offset;
    v->vector = dimension == 0 ? NULL : memory;
    memset(v->vector, 0, v->size * sizeof(uint64));
    return (Vector*)v;
}

Vector * initializeVector2(uint p, uint64 *container, uint64 *memory, uint dimension, uint offset){
    return initializeVector(&Vector2Interface, p, container, memory, dimension, offset);
}

Vector * initializeVectorGeneric(uint p, uint64 *container, uint64 *memory, uint dimension, uint offset){
    return initializeVector(&VectorGenericInterface, p, container, memory, dimension, offset);
}

// There is no case distinction between Vector2 and VectorGeneric for the functions that just 
// get and set values.
Vector * constructVector(VectorInterface *interface, uint p, uint dimension, uint offset){
    uint bit_length = getBitlength(p);
    uint size = dimension == 0 ? 0 : getLimbBitIndexPair(p, dimension + offset/bit_length - 1).limb + 1;
    uint64 * memory = malloc(
        sizeof(VectorStd) + size * sizeof(uint64)
    );
    Vector* result = initializeVector(interface, p, memory, (uint64*)((VectorStd*)memory + 1), dimension, offset);
    return result;
}

Vector * constructVector2(uint p, uint dimension, uint offset){
    return constructVector(&Vector2Interface, p, dimension, offset);
}

Vector * constructVectorGeneric(uint p, uint dimension, uint offset){
    return constructVector(&VectorGenericInterface, p, dimension, offset);
}


void freeVector(Vector * vector){
    free(vector);
}

void setVectorToZero(Vector * target){
    assert(target->offset == 0); // setToZero doesn't handle slices right now.
    VectorStd * t = (VectorStd*) target;
    memset(t->vector, 0, t->size * sizeof(uint64));
}


void assignVector(Vector * target, Vector * source){
    assert(
        source->dimension == target->dimension
        && source->offset == target->offset
        && source->offset == 0 // assignVector doesn't handle slices right now.
    );
    VectorStd * t = (VectorStd*) target;
    VectorStd * s = (VectorStd*) source;
    memcpy(t->vector, s->vector, s->size * sizeof(uint64));
}

uint unpackLimbHelper(uint *  limb_array, VectorStd* vector, uint limb_idx, uint bit_min, uint bit_max){
    uint bit_mask = getBitMask(vector->p);
    uint bit_length = getBitlength(vector->p);
    uint64 limb_value = vector->vector[limb_idx];
    uint idx = 0;
    for(uint j = bit_min; j < bit_max - bit_length + 1; j += bit_length){
        limb_array[idx] = (limb_value >> j) & bit_mask;
        idx++;
    }
    return idx;
}

uint unpackLimb(uint * limb_array, VectorStd* vector, uint limb_idx){
    uint bit_length = getBitlength(vector->p);
    uint entries_per_64_bits = getEntriesPer64Bits(vector->p);
    uint bit_min = 0;
    uint bit_max = 64;    
    if(limb_idx == 0){
        bit_min = vector->offset;
    }
    if(limb_idx == vector->size - 1){
        bit_max = (vector->offset + vector->dimension * bit_length)%(bit_length * entries_per_64_bits);;
        if(bit_max == 0){
            bit_max = 64;
        }    
    }
    return unpackLimbHelper(limb_array, vector, limb_idx, bit_min, bit_max);
}

uint packLimbHelper(VectorStd* vector, uint * limb_array, uint limb_idx, uint bit_min, uint bit_max, uint64 bit_mask){
    uint bit_length = getBitlength(vector->p);
    uint idx = 0;
    uint64 limb_value = vector->vector[limb_idx] & bit_mask;
    for(uint j = bit_min; j < bit_max - bit_length + 1; j += bit_length){
        limb_value |= ((uint64) limb_array[idx]) << j;
        idx ++;
    }
    vector->vector[limb_idx] = limb_value;
    return idx;
}

uint packLimb(VectorStd* vector, uint * limb_array, uint limb_idx){
    uint bit_length = getBitlength(vector->p);
    uint entries_per_64_bits = getEntriesPer64Bits(vector->p);
    uint bit_min = 0;
    uint bit_max = 64;    
    if(limb_idx == 0){
        bit_min = vector->offset;
    }
    if(limb_idx == vector->size - 1){
        bit_max = (vector->offset + vector->dimension * bit_length)%(bit_length * entries_per_64_bits);
        if(bit_max == 0){
            bit_max = 64;
        }    
    }
    uint64 bit_mask = 0;
    if(bit_max - bit_min < 64){
        bit_mask = (1LL << (bit_max - bit_min)) - 1;
        bit_mask <<= bit_min;
        bit_mask = ~bit_mask;
    }
    return packLimbHelper(vector, limb_array, limb_idx, bit_min, bit_max, bit_mask);
}


void packVector(Vector * target, uint * source){
    VectorStd * t = (VectorStd*) target;
    for(uint limb = 0; limb < target->size; limb++){
        source += packLimb(t, source, limb);
    }
}

void unpackVector(uint * target, Vector * source){ 
    VectorStd * s = (VectorStd*) source;    
    for(uint i = 0; i < s->size; i++){
        target += unpackLimb(target, s, i);
    }
}

uint getVectorEntry(Vector * vector, uint index){
    assert(index < vector->dimension);
    VectorStd * v = (VectorStd*) vector;    
    uint64 bit_mask = getBitMask(vector->p);
    LimbBitIndexPair limb_index = getLimbBitIndexPair(vector->p, index + v->offset);
    uint64 result = v->vector[limb_index.limb];
    result >>= limb_index.bit_index;
    result &= bit_mask;
    return result;
}

void setVectorEntry(Vector * vector, uint index, uint value){
    assert(index < vector->dimension);
    VectorStd * v = (VectorStd*) vector;    
    uint64 bit_mask = getBitMask(vector->p);
    LimbBitIndexPair limb_index = getLimbBitIndexPair(vector->p, index + v->offset);
    uint64 *result = &(v->vector[limb_index.limb]);
    *result &= ~(bit_mask << limb_index.bit_index);
    *result |= (((uint64)value) << limb_index.bit_index);
}

void sliceVector(Vector * result, Vector * source, uint start, uint end){
    assert(start <= end && end < source->dimension);
    VectorStd * r = (VectorStd*) result;
    VectorStd * s = (VectorStd*) source;
    r->dimension = end - start;
    if(start == end){
        r->size = 0;
        r->vector = NULL;
        return;    
    }
    LimbBitIndexPair limb_index = getLimbBitIndexPair(result->p, start + source->offset);
    r->offset = limb_index.bit_index;
    r->size = getVectorSize(r->p, r->dimension, r->offset);
    r->vector = s->vector + limb_index.limb;
}

VectorIterator getVectorIterator(Vector * v){
    VectorIterator result;
    if(v->dimension == 0){
        result.has_more = false;
        return result;
    }
    result.has_more = true;
    result.vector = v;
    result.index = 0;
    result.limb_index = 0;
    result.bit_index = v->offset;
    uint bit_mask = getBitMask(v->p);
    result.value = ((((VectorStd*)v)->vector[result.limb_index]) >> result.bit_index) & bit_mask;
    return result;
}

VectorIterator stepVectorIterator(VectorIterator it){
    it.index ++;
    it.has_more = it.index < it.vector->dimension;
    if(!it.has_more){
        return it;
    }
    uint bit_mask = getBitMask(it.vector->p);
    uint bit_length = getBitlength(it.vector->p);
    it.bit_index += bit_length;
    if(it.bit_index > 64 - bit_length + 1){
        it.limb_index ++;
        it.bit_index = 0;
    }
    it.value = ((((VectorStd*)(it.vector))->vector[it.limb_index]) >> it.bit_index) & bit_mask;
    return it;
}

// For the arithmetic, at 2 we xor whereas at odd primes we have to do a bit more work.

// Generic vector arithmetic
// We've chosen our packing so that we have enough space to fit p*(p-1) in each limb.
// So we do the arithmetic in place. Then we have to reduce each slot by p.
// We do this by pulling the slots out and reducing each one separately mod p via table lookup.

void addBasisElementToVectorGeneric(Vector * target, uint index, uint coeff){
    assert(index < target->dimension);
    VectorStd * t = (VectorStd*) target;    
    uint bit_mask = getBitMask(target->p);
    LimbBitIndexPair limb_index = getLimbBitIndexPair(target->p, index + target->offset);
    uint64 *result = &(t->vector[limb_index.limb]);
    uint64 new_entry = *result >> limb_index.bit_index;
    new_entry &= bit_mask;
    new_entry*= coeff;
    new_entry = modPLookup(target->p, new_entry);
    *result &= ~(bit_mask << limb_index.bit_index);
    *result |= (new_entry << limb_index.bit_index);
}

void addArrayGeneric(Vector * target, uint * source, uint c){
    VectorStd * t = (VectorStd*) target;
    uint bit_mask = getBitMask(target->p);
    uint bit_length = getBitlength(target->p);
    uint source_idx = 0;
    for(uint limb = 0; limb < target->size; limb++){
        uint64 target_limb = t->vector[limb];
        uint64 result = 0;
        uint j = 0;
        // Preserve the bits outside of the range we're writing in case target is a slice.
        if(limb == 0){
            j = target->offset;
            result |= t->vector[limb] & ((1<<target->offset) - 1);
        }
        if(limb == target->size - 1){
            result |= t->vector[limb] & (~((1<<((target->offset + target->dimension)%64)) - 1));
        }        
        for(;
            j < 64 - bit_length + 1 && source_idx < target->dimension; 
            j += bit_length
        ){
            uint64 entry = (target_limb >> j ) & bit_mask;
            entry += c*source[source_idx];
            uint64 entry_mod_p = modPLookup(target->p, entry);
            result |= entry_mod_p << j;
            source_idx ++;
        }  
        t->vector[limb] = result;
    }
}

void addVectorsGeneric(Vector * target, Vector * source, uint coeff){
    assert(
        source->dimension == target->dimension
        && target->offset == source->offset 
    );
    VectorStd * t = (VectorStd*) target;
    VectorStd * s = (VectorStd*) source;    
    uint entries[getEntriesPer64Bits(t->p)];    
    for(uint i = 0; i < source->size; i++){
        t->vector[i] = t->vector[i] + coeff * s->vector[i];
        uint limb_length = unpackLimb(entries, t, i);
        for(uint j = 0; j < limb_length; j++){
            entries[j] = modPLookup(t->p, entries[j]);
        }
        packLimb(t, entries, i);
    }
}

void scaleVectorGeneric(Vector * target, uint coeff){
    assert(coeff != 0);
    VectorStd * t = (VectorStd*) target;   
    uint entries_per_64_bits = getEntriesPer64Bits(t->p);    
    uint entries[entries_per_64_bits];
    for(uint i = 0; i < target->size; i++){
        t->vector[i] = coeff * t->vector[i];
        uint limb_length = unpackLimb(entries, t, i);
        for(uint j = 0; j < limb_length; j++){
            entries[j] = modPLookup(t->p, entries[j]);
        }
        packLimb(t, entries, i);        
    }
}

// Vector2
void addBasisElementToVector2(Vector * target, uint index, uint coeff){
    assert(index < target->dimension);
    VectorStd * t = (VectorStd*) target;    
    uint bit_mask = 1;
    LimbBitIndexPair limb_index = getLimbBitIndexPair(target->p, index + target->offset);
    uint64 *result = &(t->vector[limb_index.limb]);
    uint64 new_entry = *result >> limb_index.bit_index;
    new_entry &= bit_mask;
    new_entry *= coeff;
    *result &= ~(bit_mask << limb_index.bit_index);
    *result |= (new_entry << limb_index.bit_index);
}

void addArray2(Vector * target, uint * source, uint c){
    VectorStd * t = (VectorStd*) target;
    uint bit_length = 1;
    uint source_idx = 0;
    for(uint limb = 0; limb < target->size; limb++){
        uint64 source_limb = 0;
        for(
            uint j = (limb == 0) ? (target->offset) : 0;
            j < (64 - bit_length + 1) && source_idx < target->dimension; 
            j += bit_length
        ){
            source_limb |= source[source_idx] << j;
            source_idx ++;
        }  
        t->vector[limb] ^= source_limb;
    }
}

void addVectors2(Vector * target, Vector * source, uint coeff){
    assert(
        target->dimension == source->dimension 
        && target->offset == source->offset 
    );    
    VectorStd * t = (VectorStd*) target;
    VectorStd * s = (VectorStd*) source;    
    uint64 * target_ptr = t->vector;
    uint64 * source_ptr = s->vector;
    if(target->size == 0){
        return;
    }
    uint64 source_limb;
    if(target->size == 1){
        source_limb = *source_ptr;
        uint64 bit_mask = -1;
        uint bit_min = source->offset;
        uint bit_max = (source->offset + source->dimension) % 64;
        bit_max = bit_max ? bit_max : 64;
        if(bit_max - bit_min < 64){
            bit_mask = (1LL << (bit_max - bit_min)) - 1;
            bit_mask <<= bit_min;
        }
        source_limb &= bit_mask;
        *target_ptr ^= (coeff * source_limb);
        return;
    }
    source_limb = *source_ptr;    
    // Mask out low order bits
    source_limb &= ~((1LL<<source->offset) - 1);
    *target_ptr ^= coeff * source_limb;    
    target_ptr++;
    source_ptr++;
    for(uint i = 1; i < target->size-1; i++){
        *target_ptr ^= (coeff * (*source_ptr));
        target_ptr++;
        source_ptr++;
    }
    source_limb = *source_ptr;
    // Mask out high order bits
    uint64 bit_mask = -1;
    uint bit_max = (source->offset + source->dimension) % 64;
    if(bit_max){
        bit_mask = (1LL<<bit_max) - 1;
    }
    source_limb &= bit_mask;
    *target_ptr ^= source_limb;
}

// This is a pretty pointless method.
void scaleVector2(Vector * target, uint coeff){
    assert(coeff != 0);
    return;
    VectorStd * t = (VectorStd*) target;   
    uint64 * target_ptr = t->vector;
    for(uint i = 0; i < target->size; i++){
        *target_ptr *= coeff;
        target_ptr++;
    }
}

VectorInterface VectorGenericInterface = {
    (sizeof(VectorStd) + 7)/8,
    getEntriesPer64Bits, getVectorSize, initializeVectorGeneric,
    constructVectorGeneric, freeVector,    
    assignVector, setVectorToZero, packVector, unpackVector,
    getVectorEntry, setVectorEntry,
    sliceVector, getVectorIterator, stepVectorIterator,    
    addBasisElementToVectorGeneric, addArrayGeneric, addVectorsGeneric, scaleVectorGeneric,
};

VectorInterface Vector2Interface = {
    (sizeof(VectorStd) + 7)/8,
    getEntriesPer64Bits, getVectorSize, initializeVector2,    
    constructVector2, freeVector,    
    assignVector, setVectorToZero, packVector, unpackVector,
    getVectorEntry, setVectorEntry,
    sliceVector, getVectorIterator, stepVectorIterator,
    addBasisElementToVector2, addArray2, addVectors2, scaleVector2,
};

uint vectorToString(char * buffer, Vector * vector){
    uint len = 0;
    len += sprintf(buffer + len, "[");
    for(VectorIterator it = getVectorIterator(vector); it.has_more; it = stepVectorIterator(it)){
        len += sprintf(buffer + len, "%d, ", it.value);
    }
    len += sprintf(buffer + len, "]");
    return len;
}

void printVector(uint p, Vector * v){
    char buffer[200];
    vectorToString(buffer, v);
    printf("%s\n", buffer);
}



/**/
int main(int argc, char *argv[]){
    // uint p = 3;
    // uint bitmask = 3;
    // uint dim = 49;
    // uint num_vectors = 100;
    // uint scale = 3;    
    // uint num_repeats = 400;
    
    // initializePrime(p);
    // uint arrays[num_vectors][dim];
    // uint container_size = VectorGenericInterface.container_size;
    // uint contents_size = VectorGenericInterface.getSize(p, dim, 0);
    // uint64 vector_container_memory[container_size * num_vectors];
    // uint64 vector_contents_memory[contents_size * num_vectors];
    // uint64 * vector_container_ptr = vector_container_memory;
    // uint64 * vector_contents_ptr = vector_contents_memory;
    // Vector * vectors[num_vectors];
    // for(uint i = 0; i < num_vectors; i++){
    //     for(uint j = 0; j < dim; j++){
    //         arrays[i][j] = modPLookup(p, rand() & bitmask);
    //     }
    // }
    // for(uint i = 0; i < num_vectors; i++){
    //     vectors[i] = VectorGenericInterface.initialize(p, vector_container_ptr, vector_contents_ptr, dim, 0);
    //     vector_container_ptr += container_size;
    //     vector_contents_ptr += contents_size;
    // }
    
    // // for(uint i = 0; i < num_vectors; i++){
    // //     packVector(vectors[i], arrays[i]);
    // // }

    // char buffer[1000];
    // array_to_string(buffer, arrays[1], dim);
    // printf("array:     %s\n", buffer);

    // printf("Packing    ");
    // packVector(vectors[1], arrays[1]);
    // printf("\n");
    // for(uint i = 0; i < dim; i++){
    //     printf("%d ", getVectorEntry(vectors[1], i));
    // }
    // printf("\n");
    // vectorToString(buffer, vectors[1]);
    // printf("vector:    %s\n", buffer);
    // unpackVector(arrays[1], vectors[1]);
    // array_to_string(buffer, arrays[1], dim);
    // printf("unpacked:  %s\n", buffer);

    // return 0;
    // sliceVector(vectors[1], vectors[0], 2, 4);


    // Vector * v = constructVector(&VectorGenericInterface, p, 4 - 2, vectors[1]->offset);
    // packVector(v, arrays[3]);
    // vectorToString(buffer, v);
    // printf("v:               %s\n", buffer);

    // vectorToString(buffer, vectors[0]);
    // printf("vector: %s\n", buffer);
    // vectorToString(buffer, vectors[1]);
    // printf("slice:           %s\n", buffer);
    
    // addVectorsGeneric(vectors[1], v, 1);
    // vectorToString(buffer, vectors[1]);
    // printf("sum:             %s\n", buffer);
    // vectorToString(buffer, vectors[0]);
    // printf("vector: %s\n", buffer);    

    // printf("argc : %d\n", argc);
    // if(argc > 1){
    //     printf("Testing addVectors\n");
    //     for(uint repeats = 0; repeats < num_repeats; repeats ++){
    //         for(uint i = 0; i < num_vectors; i++){
    //             for(uint j = 0; j < num_vectors; j++){
    //                 if(i==j){
    //                     continue;
    //                 }
    //                 addVectorsGeneric(p, vectors[i], vectors[j], scale);
    //             }
    //         }
    //     }
    // } else {
    //     printf("Comparison\n");
    //     for(uint repeats = 0; repeats < num_repeats; repeats ++){
    //         for(uint i = 0; i < num_vectors; i++){
    //             for(uint j = 0; j < num_vectors; j++){
    //                 if(i==j){
    //                     continue;
    //                 }
    //                 for(uint k = 0; k < dim; k++){
    //                     arrays[i][k] += arrays[j][k] * scale;
    //                     arrays[i][k] = arrays[i][k] % p;
    //                 }
    //             }
    //         }   
    //     }
    // }

    // array_to_string(buffer, arrays[0], 10);
    // printf("%s\n\n", buffer);
    // vectorToString(buffer, p, vectors[0]);
    // printf("%s\n\n", buffer);
    
    // #define ROWS 5
    // #define COLS 10
    // uint p = 5;

    #define ROWS 2
    #define COLS 3
    uint p = 3;
    initializePrime(p);
    Vector ** M = constructMatrix(&VectorGenericInterface, p, ROWS, COLS);
    uint array[ROWS][COLS] = {{1,2,1},{1,1,0}};
    // uint array[ROWS][COLS] = {
    //     {1, 0, 0, 4, 0, 2, 1, 3, 0, 0},
    //     {4, 3, 2, 3, 4, 3, 4, 2, 4, 4},
    //     {3, 0, 1, 2, 3, 4, 4, 3, 1, 3},
    //     {0, 1, 3, 0, 2, 1, 3, 0, 2, 3},
    //     {4, 4, 3, 4, 2, 4, 0, 0, 0, 3}
    // };
    for(uint i = 0; i < ROWS; i++){
        packVector(M[i], array[i]);
    }        
    printMatrix(M, ROWS);  


    int pivots[COLS];
    rowReduce(M, pivots, ROWS);
    printf("M: ");
    printMatrix(M, ROWS);
    return 0;
}
/**/

uint getMatrixSize(VectorInterface * vectImpl, uint p, uint rows, uint cols){
    return rows 
        + rows * vectImpl->container_size 
        + rows * vectImpl->getSize(p, cols, 0);
}

Vector** initializeMatrix(uint64 * memory, VectorInterface * vectImpl, uint p, uint rows, uint cols)  {
    uint container_size = vectImpl->container_size;
    uint vector_size = vectImpl->getSize(p, cols, 0);
    Vector ** vector_ptr = (Vector**)memory;
    uint64 * container_ptr = (uint64*)(memory + rows);
    uint64 * values_ptr = container_ptr + rows * container_size;
    for(int row = 0; row < rows; row++){
        *vector_ptr = vectImpl->initialize(p, container_ptr, values_ptr, cols, 0);
        vector_ptr ++;
        container_ptr += container_size;
        values_ptr += vector_size;
    }
    return (Vector**)memory;
}

Vector** constructMatrix(VectorInterface * vectorImplementation, uint p, uint rows, uint cols)  {
    uint64 * M = malloc(getMatrixSize(vectorImplementation, p, rows, cols) * sizeof(uint64));
    return initializeMatrix(M, vectorImplementation, p, rows, cols);
}

Vector** constructMatrixGeneric(uint p, uint rows, uint cols)  {
    return constructMatrix(&VectorGenericInterface, p, rows, cols);
}

Vector** constructMatrix2(uint p, uint rows, uint cols)  {
    return constructMatrix(&Vector2Interface, p, rows, cols);
}

uint matrixToString(char * buffer, Vector **M, uint rows){
    int len = 0;
    len += sprintf(buffer + len, "    [\n");
    for(int i = 0; i < rows; i++){
        len += sprintf(buffer + len, "        ");
        len += vectorToString(buffer + len, M[i]);
        len += sprintf(buffer + len, ",\n");
    }
    len += sprintf(buffer + len, "    ]\n");
    return len;
}

void printMatrix(Vector **matrix, uint rows){
    char buffer[10000];
    matrixToString(buffer, matrix, rows);
    printf("%s\n", buffer);
}

void rowReduce(Vector ** matrix, int * column_to_pivot_row, uint rows){
    VectorInterface * vectorImpl = matrix[0]->interface;
    uint p = matrix[0]->p;
    uint columns = matrix[0]->dimension;
    VectorIterator rowIterators[rows];
    for(uint i = 0; i < rows; i++){
        rowIterators[i] = vectorImpl->getIterator(matrix[i]);
    }
    memset(column_to_pivot_row, -1, columns * sizeof(uint));
    uint pivot = 0;
    for(uint pivot_column = 0; pivot_column < columns; pivot_column++){
        // Search down column for a nonzero entry.
        uint pivot_row;
        for(pivot_row = pivot; pivot_row < rows; pivot_row++){
            if(rowIterators[pivot_row].value != 0){
                break;
            }
        }
        // No pivot in pivot_column.
        if(pivot_row == rows){
            for(uint i = 0; i < rows; i++){
                rowIterators[i] = vectorImpl->stepIterator(rowIterators[i]);
            }
            continue;
        }
        // Record position of pivot.
        column_to_pivot_row[pivot_column] = pivot_row;

        printMatrix(matrix, rows);

        // Pivot_row contains a row with a pivot in current column.
        // Swap pivot row up.
        Vector* temp = matrix[pivot];
        matrix[pivot] = matrix[pivot_row];
        matrix[pivot_row] = temp;

        VectorIterator temp_it = rowIterators[pivot];
        rowIterators[pivot] = rowIterators[pivot_row];
        rowIterators[pivot_row] = temp_it;

        printf("row(%d) <==> row(%d)\n", pivot, pivot_row);
        printMatrix(matrix, rows);

        // Divide pivot row by pivot entry
        int c = rowIterators[pivot].value;
        int c_inv = inverse(p, c);
        vectorImpl->scale(matrix[pivot], c_inv);

        printf("row(%d) *= %d\n", pivot, c_inv);
        printMatrix(matrix, rows);
        for(int i = 0; i < rows; i++){
            // Between pivot and pivot_row, we already checked that the pivot column is 0, so skip ahead a bit.
            if(i == pivot){
                i = pivot_row;
                continue;
            }
            int pivot_column_entry = rowIterators[i].value;
            int row_op_coeff = (p - pivot_column_entry) % p;
            if(row_op_coeff == 0){
                continue;
            }
            // Do row operation
            vectorImpl->add(matrix[i], matrix[pivot], row_op_coeff);

            printf("row(%d) <== row(%d) + %d * row(%d)\n", i, i, row_op_coeff, pivot);
            printMatrix(matrix, rows);
        }
        pivot ++;
        for(uint i = 0; i < rows; i++){
            rowIterators[i] = vectorImpl->stepIterator(rowIterators[i]);
        }        
    }
    return;
}