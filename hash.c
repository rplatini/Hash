#define _POSIX_C_SOURCE 200809L
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPACIDAD_INICIAL 29
#define FACTOR_CARGA_MAX 0.7
#define FACTOR_CARGA_MIN 0.2
#define CAPACIDAD_MENOR 0.75
#define CAPACIDAD_MAYOR 5
#define NO_ENCONTRADO -1

//DEFINICION DE ESTRUCTURAS

typedef enum{
    VACIO,
    OCUPADO, 
    BORRADO, 
} estado_t;

typedef struct campo {
    char *clave;
    void *dato;
    estado_t estado;
} campo_t;

struct hash {
    campo_t* tabla;
    size_t capacidad;
    size_t cantidad;
    hash_destruir_dato_t destruir_dato;
};

struct hash_iter {
    const hash_t* hash;
    size_t posicion_actual;
};

// funcion de hash: https://en.wikipedia.org/wiki/Jenkins_hash_function

size_t fhash(const char* key, size_t largo) {
    size_t length = strlen(key);
    size_t i = 0;
    size_t hash = 0;
    while (i != length) {
        hash += (size_t)key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return (hash % largo);
}


// pre: el hash fue creado
// post: retorna el factor de carga del hash
double factor_de_carga(hash_t* hash){
    return (double)(hash->cantidad / hash->capacidad);
}


// inicializa una tabla
void inicializar_tabla(hash_t* hash){
    for(size_t i = 0; i < hash->capacidad; i++){
        hash->tabla[i].clave = NULL;
        hash->tabla[i].dato = NULL;
        hash->tabla[i].estado = VACIO;
    }
}


// funcion de redimension

bool hash_redimensionar(hash_t* hash, size_t nueva_capacidad){
    campo_t* tabla_vieja = hash->tabla;
    size_t capacidad_vieja = hash->capacidad;

    campo_t* tabla_redimension = malloc(sizeof(campo_t) * nueva_capacidad);
    if (!tabla_redimension){
        return false;
    }

    hash->tabla = tabla_redimension;
    hash->capacidad = nueva_capacidad;
    hash->cantidad = 0;
    inicializar_tabla(hash);

    for(size_t i = 0; i < capacidad_vieja; i++){
        if(tabla_vieja[i].estado == OCUPADO){
            hash_guardar(hash, tabla_vieja[i].clave, tabla_vieja[i].dato);
            free(tabla_vieja[i].clave);
        }
    }

    free(tabla_vieja);
    return true;
}

//PRIMITIVAS HASH

hash_t *hash_crear(hash_destruir_dato_t destruir_dato){
    hash_t* hash = malloc(sizeof(hash_t));
    if(!hash) return NULL;

    campo_t* tabla = malloc(sizeof(campo_t)*CAPACIDAD_INICIAL);
    if(!tabla){
        free(hash);
        return NULL;
    }

    hash->tabla = tabla;
    hash->capacidad = CAPACIDAD_INICIAL;
    hash->cantidad = 0;
    hash->destruir_dato = destruir_dato;
    inicializar_tabla(hash);
    
    return hash;
}


// pre: el hash esta inicializado, la clave tambien
// post: - devuelve la posicion en el hash de la clave buscada.
//       - si la clave no se encuentra, la posicion en la que deberia estar.
size_t posicion_clave(const hash_t *hash, const char *clave, bool* encontrado){
    size_t posicion = fhash(clave, (size_t)((unsigned int)(hash->capacidad)));

    while (hash->tabla[posicion].estado != VACIO){

        if ((hash->tabla[posicion].estado == OCUPADO) && (strcmp(hash->tabla[posicion].clave, clave) == 0)){
            *encontrado = true;
            return posicion;
        }
        posicion++;
        if (posicion == hash->capacidad) posicion = 0;
    }
    *encontrado = false;
    return posicion;
}


bool hash_pertenece(const hash_t *hash, const char *clave){
    bool encontrado;
    posicion_clave(hash, clave, &encontrado);
    return encontrado;
}


void *hash_obtener(const hash_t *hash, const char *clave){
    bool encontrado;
    size_t posicion = posicion_clave(hash, clave, &encontrado);

    return encontrado ? hash->tabla[posicion].dato : NULL;
}


// guarda un elemento de tipo campo_t en el hash
void guardar(hash_t *hash, const char *clave, void* dato, size_t posicion){
    char* copia_clave = strdup(clave);
    if(!copia_clave) return;
    hash->tabla[posicion].clave = copia_clave;
    hash->tabla[posicion].dato = dato;
    hash->tabla[posicion].estado = OCUPADO;
}


bool hash_guardar(hash_t *hash, const char *clave, void *dato){
    if (factor_de_carga(hash) > FACTOR_CARGA_MAX){
        if (!hash_redimensionar(hash, hash->capacidad *(size_t)CAPACIDAD_MAYOR)){
            return false;
        }
    }
    bool encontrado;
    size_t posicion = posicion_clave(hash, clave, &encontrado);
    if (encontrado){
        void* borrado = hash->tabla[posicion].dato;

        hash->tabla[posicion].dato = dato;
        if (hash->destruir_dato){
            hash->destruir_dato(borrado);
        }

    } else {
        guardar(hash, clave, dato, posicion);
        hash->cantidad++;
    }

    return true;
}


void *hash_borrar(hash_t * hash, const char *clave){
    bool encontrado;
    size_t posicion = posicion_clave(hash, clave, &encontrado);
    if (!encontrado) return NULL;

    if (hash->cantidad == (hash->capacidad*(size_t)CAPACIDAD_MENOR)){
        size_t capacidad_nueva = (hash->capacidad /CAPACIDAD_MAYOR);
        if(!hash_redimensionar(hash, capacidad_nueva)) return NULL;
    }

    void *auxiliar = hash->tabla[posicion].dato;

    hash->tabla[posicion].dato = NULL;
    hash->tabla[posicion].estado = BORRADO;
    free(hash->tabla[posicion].clave);
    hash->cantidad--;

    return auxiliar;
}


size_t hash_cantidad(const hash_t *hash){
    return hash->cantidad;
}


void hash_destruir(hash_t *hash){
    for (size_t i = 0; i < hash->capacidad; i++){
        if(hash->tabla[i].estado == OCUPADO){
            free(hash->tabla[i].clave);

            if (hash->destruir_dato != NULL){
                hash->destruir_dato(hash->tabla[i].dato);
            }
        }
    }
    free(hash->tabla);
    free(hash);
}

// PRIMITIVAS ITERADOR HASH 

bool iter_puede_avanzar(hash_iter_t *iter){
    while(!hash_iter_al_final(iter)){
        if(iter->hash->tabla[iter->posicion_actual].estado == OCUPADO){
            return true;
        }
        iter->posicion_actual++;
    } 
    return false;
}

hash_iter_t* hash_iter_crear(const hash_t* hash){
    hash_iter_t* iter = malloc(sizeof(hash_iter_t));
    if(!iter) return NULL;

    iter->hash = hash;
    iter->posicion_actual = 0;
    if (!iter_puede_avanzar(iter)){
        iter->posicion_actual = iter->hash->capacidad;
    }
    return iter;
}


bool hash_iter_avanzar(hash_iter_t *iter){
    if(hash_iter_al_final(iter)) return false;
    iter->posicion_actual++;

    return iter_puede_avanzar(iter);
}


const char *hash_iter_ver_actual(const hash_iter_t *iter){
    if(hash_iter_al_final(iter)) return NULL;
    return iter->hash->tabla[iter->posicion_actual].clave;
}


bool hash_iter_al_final(const hash_iter_t *iter){
    return (iter->posicion_actual == iter->hash->capacidad);
}


void hash_iter_destruir(hash_iter_t *iter){
    free(iter);
}

