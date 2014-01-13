#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "geo.h"

// This will eventually come from zlib.h
int uncompress(void *, int *, void *, int);

// Extended data structure for obscuring control information from applications
typedef struct {
    GEO            geo;
    unsigned char *data;
    int            len;
} GEO_EXT;

// Macros to take the place of common functions
#define GetInt16(x, y) ( \
    ((int) x[y + 1] <<  8) | ((int) x[y]) )
#define GetInt32(x, y) ( \
    ((int) x[y + 3] << 24) | ((int) x[y + 2] << 16) | \
    ((int) x[y + 1] <<  8) | ((int) x[y]) )

// Global data
static int GEO_VERBOSE = 0;



////////////////////////////////////////////////////////////////////////////////
//                             Non-API Functions                              //
////////////////////////////////////////////////////////////////////////////////

// Extract a zlib stream
static unsigned char* getZlib(unsigned char *data, int packed, int unpacked) {
    unsigned char *ret;
    int err1, err2;

    // Check if there is any data to load
    if (!unpacked) return NULL;

    // Check if the data is uncompressed
    if (!packed) {
        ret = malloc(unpacked);
        memcpy(ret, data, unpacked);
        return ret;
    }

    // Attempt to decompress the data
    ret = malloc(unpacked);
    err2 = unpacked;
    err1 = uncompress(ret, &err2, data, packed);
    if (err1 || err2 != unpacked) { free(ret); return NULL; }

    // Return the decompressed data
    return ret;
}

// Unpack the meta stream from the file data
static unsigned char* getMeta(unsigned char *data, 
    int *len, int *offset, int *version) {
    int MetaSize, UnpackedSize;
    unsigned char *meta;
    int zipbias = 12;

    // Parameters are validated by geoLoad()
    int dumb = 16;

    // Read data from file header
    MetaSize     = GetInt32(data,  0);
    UnpackedSize = GetInt32(data,  4);
    if (UnpackedSize == 0) {
    	*version     = GetInt32(data,  8);
    	UnpackedSize = GetInt32(data, 12);
    } else {
	*version = 0;
	dumb -= 8;
	zipbias = 4;
    }

    // Check header for errors
    if (MetaSize < 12 || MetaSize + 4 > *len || 
        !UnpackedSize) return NULL;


    // Extract the meta stream
    meta = getZlib(&data[dumb], MetaSize - zipbias, UnpackedSize);
    if (meta == NULL) return NULL;

    // Return the extracted chunk
    *offset = MetaSize + (*version == 0 ? 8 : 4);
    *len = UnpackedSize;
    if (version == 0)
	    *offset += 4;
    return meta;
}

// Decoder function to process data references
static void* refDecode(
    unsigned char *data, int len, int count, int members, int mode) {
    int tlen, x, y, offset, bits, *iaccum;
    float exponent, value, *faccum;
    void **values;
    char *types;

    // Error checking
    if (data == NULL || !len || !count || !members || 
        mode < 0 || mode > 2) return NULL;

    // Determine number of values and bytes for type list
    count *= members;
    tlen = (count >> 2) + ((count & 3) ? 1 : 0);
    if (tlen + 1 > len) return NULL;

    // Process the list of types
    types = malloc(count);
    for (x = offset = y = 0; x < count; x++) {

        // Process the current value in the list
        if (!(x & 3)) bits = data[offset++];
        types[x] = bits & 3;
        bits >>= 2;

        // Calculate how many more bytes are needed to decode the value
        y += (8 >> (4 - types[x]));

    } // x

    // Check if there are enough bytes left to proceed
    if (tlen + y + 1 > len) { free(types); return NULL; }

    // Allocate memory based on data type mode
    if (!mode) { // Float
        if (data[tlen] > 31) { free(types); return NULL;}
        exponent = (float) (1 << (int) data[tlen]);
        faccum = calloc(members * sizeof(float), 1);
        values = malloc(count   * sizeof(float));
    } else {     // Int
        iaccum = calloc(members * sizeof(int), 1);
        values = malloc(count * 
            ((mode == 1) ? sizeof(int) : sizeof(short)));
    }

    // Load the values from the data depending on type
    offset = tlen + 1;
    for (x = 0; x < count; x++) {
        switch (types[x]) {

        // Use the previous value
        case 0:
            if (!mode) value = 0.0f;
            else       bits  = 1;
            break;

        // Load an 8-bit float
        case 1:
            bits = (int) data[offset] - 0x7F;
            offset++;
            if (!mode) value = (float) bits / exponent;
            else bits++;
            break;

        // Load a 16-bit float
        case 2:
            bits = GetInt16(data, offset) - 0x7FFF;
            offset += 2;
            if (!mode) value = (float) bits / exponent;
            else bits++;
            break;

        // Load a 32-bit value
        case 3:
            bits = GetInt32(data, offset);
            offset += 4;
            if (!mode) value = *(float *)&bits;
            else bits++;
            break;

        default:;
        } // switch

        // Apply the value to the return list
        y = x % members;
        if (!mode) {
            faccum[y] += value;
            ((float *)values)[x] = faccum[y];
        } else {
            bits = iaccum[y] += bits;
            if (mode == 2) ((short *)values)[x] = (short) (bits & 0xFFFF);
            else           ((int   *)values)[x] = bits;
        }

    } // x

    // Clean up memory and return list of loaded values
    free(types);
    if (!mode) free(faccum);
    else       free(iaccum);
    return values;
}

// Loads one model from a GEO meta stream
static int getModel(GEO_MODEL *mod, unsigned char *data, int offset, 
    unsigned char *names, int namelen, unsigned char *pool, int poollen, int version) {
    int x, y, z, packed, unpacked, pooloff, *indexes;
    float *coords, *normals, *texcoords;
    unsigned char *refdata;

    // Check the size of the data structure
    x = GetInt32(data, offset); offset += 4;
 /*   if (x != 0xF4) {
        if (GEO_VERBOSE)
            printf("ERROR: Unsupported model block size:  0x%08X\n", x);
        return 1;
    } */

    // Load data about model
    offset += 12; // unk1, unk2, unk3
    mod->vertexnum = GetInt32(data, offset); offset += 4;
    mod->facenum   = GetInt32(data, offset); offset += 4;
    if (version < 8) offset -= 4;
    offset += 40; // unk6 through unk15
    if (mod->vertexnum) mod->vertices = 
        malloc(mod->vertexnum * sizeof(GEO_VERTEX));
    if (mod->facenum) mod->faces = 
        malloc(mod->facenum * sizeof(GEO_FACE));

    // Load model name
    x = GetInt32(data, offset); offset += 4;
    if (x < 0 || x >= namelen) {
        if (GEO_VERBOSE)
            printf("ERROR: Invalid model name offset encountered\n");
        return 1;
    }
    mod->id = &names[x];

    // Load more data
    offset += 40; // unk17 through Bounds2

    // Prepare face and vertex data arrays
    indexes = NULL; coords = normals = texcoords = NULL;

    // Load faces
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        indexes = (int *) refDecode(refdata, unpacked, mod->facenum, 3, 1);
        free(refdata);
    }

    // Load vertex coordinates
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        coords = (float *) refDecode(refdata, unpacked, mod->vertexnum, 3, 0);
        free(refdata);
    }

    // Load vertex normals
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        normals = (float *) refDecode(refdata, unpacked, mod->vertexnum, 3, 0);
        free(refdata);
    }

    // Load texture coordinates
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        texcoords = (float *) 
            refDecode(refdata, unpacked, mod->vertexnum, 2, 0);
        free(refdata);
    }

    // Check if everything loaded correctly
    if (indexes == NULL || coords == NULL || 
        normals == NULL || texcoords == NULL) {
        if (indexes   != NULL) free(indexes);
        if (coords    != NULL) free(coords);
        if (normals   != NULL) free(normals);
        if (texcoords != NULL) free(texcoords);
        if (GEO_VERBOSE)
            printf("ERROR: Could not unpack data for %s\n", mod->id);
        return 1;
    }

    // Process faces
    for (x = 0; x < mod->facenum; x++) {
        for (y = 0; y < 3; y++) {

            // Get the next vertex index
            z = indexes[x * 3 + y];
            if (z < 0 || z >= mod->vertexnum) {
                free(indexes); free(coords); free(normals); free(texcoords);
                if (GEO_VERBOSE)
                    printf("ERROR: Invalid vertex index in %s\n", mod->id);
                return 1;
            }

            // Assign the vertex index
            if (y == 0) mod->faces[x].v1 = z;
            if (y == 1) mod->faces[x].v2 = z;
            if (y == 2) mod->faces[x].v3 = z;
        } // y
    } // x
    free(indexes);

    // Process vertex coordinates
    for (x = y = 0; x < mod->vertexnum; x++) {
        mod->vertices[x].x = coords[y++];
        mod->vertices[x].y = coords[y++];
        mod->vertices[x].z = coords[y++];
    }
    free(coords);

    // Process vertex normals
    for (x = y = 0; x < mod->vertexnum; x++) {
        mod->vertices[x].nx = normals[y++];
        mod->vertices[x].ny = normals[y++];
        mod->vertices[x].nz = normals[y++];
    }
    free(normals);

    // Process texture coordintes
    for (x = y = 0; x < mod->vertexnum; x++) {
        mod->vertices[x].s = texcoords[y++];
        mod->vertices[x].t = texcoords[y++];
    }
    free(texcoords);

    // Return success
    return 0;
}

static int getModelv2(GEO_MODEL *mod, unsigned char *data, int offset, 
    unsigned char *names, int namelen, unsigned char *pool, int poollen, int version) {
    int x, y, z, packed, unpacked, pooloff, *indexes;
    float *coords, *normals, *texcoords;
    unsigned char *refdata;

   /* {
	    int i;
	    for (i = offset; i < offset + 200; i += 4) {
		    int dval = GetInt32(data,i);
		    printf("+%d (%d): %d or %g\n", i - offset, i, dval, *(float*)&dval);
	    }
    } */

    // Load data about model
    offset += 28; // unk1, unk2, unk3
    mod->vertexnum = GetInt32(data, offset); offset += 4;
    mod->facenum   = GetInt32(data, offset); offset += 4;
    offset += 44; // unk6 through unk15
    if (mod->vertexnum) mod->vertices = 
        malloc(mod->vertexnum * sizeof(GEO_VERTEX));
    if (mod->facenum) mod->faces = 
        malloc(mod->facenum * sizeof(GEO_FACE));

    // Load model name
    x = GetInt32(data, offset); offset += 4;
    if (x < 0 || x >= namelen) {
        if (GEO_VERBOSE)
            printf("ERROR: Invalid model name offset encountered\n");
        return 1;
    }
    mod->id = &names[x];

    // Load more data
    offset += 48; // unk17 through Bounds2

    // Prepare face and vertex data arrays
    indexes = NULL; coords = normals = texcoords = NULL;

    // Load faces
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        indexes = (int *) refDecode(refdata, unpacked, mod->facenum, 3, 1);
        free(refdata);
    }

    // Load vertex coordinates
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        coords = (float *) refDecode(refdata, unpacked, mod->vertexnum, 3, 0);
        free(refdata);
    }

    // Load vertex normals
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        normals = (float *) refDecode(refdata, unpacked, mod->vertexnum, 3, 0);
        free(refdata);
    }

    // Load texture coordinates
    packed   = GetInt32(data, offset); offset += 4;
    unpacked = GetInt32(data, offset); offset += 4;
    pooloff  = GetInt32(data, offset); offset += 4;
    refdata = getZlib(&pool[pooloff], packed, unpacked);
    if (refdata != NULL) {
        texcoords = (float *) 
            refDecode(refdata, unpacked, mod->vertexnum, 2, 0);
        free(refdata);
    }

    // Check if everything loaded correctly
    if (indexes == NULL || coords == NULL || 
        normals == NULL || texcoords == NULL) {
        if (indexes   != NULL) free(indexes);
        if (coords    != NULL) free(coords);
        if (normals   != NULL) free(normals);
        if (texcoords != NULL) free(texcoords);
        if (GEO_VERBOSE)
            printf("ERROR: Could not unpack data for %s\n", mod->id);
        return 1;
    }

    // Process faces
    for (x = 0; x < mod->facenum; x++) {
        for (y = 0; y < 3; y++) {

            // Get the next vertex index
            z = indexes[x * 3 + y];
            if (z < 0 || z >= mod->vertexnum) {
                free(indexes); free(coords); free(normals); free(texcoords);
                if (GEO_VERBOSE)
                    printf("ERROR: Invalid vertex index in %s\n", mod->id);
                return 1;
            }

            // Assign the vertex index
            if (y == 0) mod->faces[x].v1 = z;
            if (y == 1) mod->faces[x].v2 = z;
            if (y == 2) mod->faces[x].v3 = z;
        } // y
    } // x
    free(indexes);

    // Process vertex coordinates
    for (x = y = 0; x < mod->vertexnum; x++) {
        mod->vertices[x].x = coords[y++];
        mod->vertices[x].y = coords[y++];
        mod->vertices[x].z = coords[y++];
    }
    free(coords);

    // Process vertex normals
    for (x = y = 0; x < mod->vertexnum; x++) {
        mod->vertices[x].nx = normals[y++];
        mod->vertices[x].ny = normals[y++];
        mod->vertices[x].nz = normals[y++];
    }
    free(normals);

    // Process texture coordintes
    for (x = y = 0; x < mod->vertexnum; x++) {
        mod->vertices[x].s = texcoords[y++];
        mod->vertices[x].t = texcoords[y++];
    }
    free(texcoords);

    // Return success
    return 0;
}

// Loads the models within a GEO meta stream
static int getModels(GEO_EXT *geox, 
    unsigned char *pool, int len, int version) {
    int PoolSize, TexNamesSize, ModNamesSize, TexEnumsSize;
    unsigned char *blockdata;
    GEO *geo = &geox->geo;
    int x, y, offset = 16, blocksize;
    int fix = 0;
    int lodsize = 0;
    int tex, texcount;

    // Check if a full header exists
    if (geox->len < 16) {
        if (GEO_VERBOSE)
            printf("ERROR: Insufficient data for meta header\n");
        return 1;
    }

    // Read header information
    PoolSize     = GetInt32(geox->data,  0);
    TexNamesSize = GetInt32(geox->data,  4);
    ModNamesSize = GetInt32(geox->data,  8);
    TexEnumsSize = GetInt32(geox->data, 12);

    // Check header for errors
    if (PoolSize > len || TexNamesSize < 4 || !ModNamesSize || !TexEnumsSize || 
        TexNamesSize + ModNamesSize + TexEnumsSize + 16 > geox->len) {
        if (GEO_VERBOSE)
            printf("ERROR: Meta stream header contains invalid data\n");
        return 1;
    }

    if (version >= 2 && version <= 6) {
	    lodsize = GetInt32(geox->data, 16);
	    offset += 4;
	    fix = 4;
    }

    // Load information about texture filenames
    geo->texturenum = GetInt32(geox->data, offset); offset += 4;
    blocksize = TexNamesSize - geo->texturenum * 4 - 4;
    blockdata = &geox->data[16 + fix + TexNamesSize - blocksize];

    // Load texture filenames
    if (geo->texturenum) geo->textures = 
        malloc(geo->texturenum * sizeof(void *));
    for (x = 0; x < geo->texturenum; x++) {

        // Check if the offset is valid
        y = GetInt16(geox->data, offset); offset += 4;
        if (y < 0 || y >= blocksize) {
            if (GEO_VERBOSE)
                printf("ERROR: Invalid texture name offset encountered\n");
            return 1;
        }

        // Assign the texture name
        geo->textures[x] = &blockdata[y];
    }

    // Load information about models
    blockdata = &geox->data[16 + fix + TexNamesSize];
    offset = TexNamesSize + ModNamesSize + TexEnumsSize + lodsize + fix + 16;
    geo->id = &geox->data[offset]; offset += 0x84;
    offset += 4; // unk1
    geo->modelnum = GetInt32(geox->data, offset); offset += 4;
    if (!geo->modelnum) return 0; // Nothing left to do

    // Load models
    geo->models = calloc(geo->modelnum * sizeof(GEO_MODEL), 1);
    for (x = 0; x < geo->modelnum; x++) {

        // Check if there's enough room for another model
        if (offset <= geox->len - 4) {
	    if (version < 3)
		y = 0xD8;
	    else
        	y = GetInt32(geox->data, offset);
	}
        if (offset > geox->len - y) {
            if (GEO_VERBOSE)
                printf("ERROR: Unexpected end of data\n");
            return 1;
        }

        // Load the model
	if (version < 3) {
	        if (getModelv2(&geo->models[x], geox->data, offset, 
	            blockdata, ModNamesSize, pool, len, version))
	            return 1; // An error coccurred
	} else {
	        if (getModel(&geo->models[x], geox->data, offset, 
	            blockdata, ModNamesSize, pool, len, version))
	            return 1; // An error coccurred
	}
        offset += y;
    }

    // Assign textures to faces
    blockdata = &geox->data[16 + fix + TexNamesSize + ModNamesSize];
    blocksize = TexEnumsSize;

    for (x = offset = texcount = 0; x < geo->modelnum; x++) {
        for (y = 0; y < geo->models[x].facenum; y++) {
            // Load information for the next texture
            if (!texcount) {
                if (offset > blocksize - 4) {
                    if (GEO_VERBOSE)
                        printf("WARNING: Unexpected end of texture enums\n");
		    tex = 0;
                } else {
	                tex      = GetInt16(blockdata, offset); offset += 2;
	                texcount = GetInt16(blockdata, offset); offset += 2;
	                if (tex < 0 || tex >= geo->texturenum) {
	                    if (GEO_VERBOSE) {
	                        printf("ERROR: Invalid texture");
	                        printf(" enum index encountered\n");
	                    }  return 1;
        	        }
		}
            }

            // Assign the texture to the face
            geo->models[x].faces[y].texture = tex;
            texcount--;

        } // y
    } // x

    // Return success
    return 0;
}



////////////////////////////////////////////////////////////////////////////////
//                               API Functions                                //
////////////////////////////////////////////////////////////////////////////////

// Load a GEO file into a GEO structure
GEO* geoLoad(unsigned char *data, int len) {
    unsigned char *pool;
    GEO_EXT *geox;
    int offset, version;
    GEO *geo;

    // Error checking
    if (data == NULL || len < 16) {
        if (GEO_VERBOSE)
            printf("ERROR: Bad parameters passed to geoLoad()\n");
        return NULL;
    }

    // Unpack the meta stream from the data
    geox = calloc(sizeof(GEO_EXT), 1);
    geox->len = len;
    geox->data = getMeta(data, &geox->len, &offset, &version);
    printf("Version: %d\n", version);
    if (geox->data == NULL) {
        geoFree(&geox->geo);
        if (GEO_VERBOSE)
            printf("ERROR: Unsupported .geo container format\n");
        return NULL;
    }
    pool = &data[offset];

    // Extract models
    if (getModels(geox, pool, len - offset, version)) {
        geoFree(&geox->geo);
        return NULL;
    }

    // Return the loaded GEO object
    return &geox->geo;
}

// Delete a GEO structure
void geoFree(GEO *geo) {
    GEO_EXT *geox;
    int x;

    // Error checking
    if (geo == NULL) {
        if (GEO_VERBOSE)
            printf("WARNING: Argument passed to geoFree() was NULL\n");
        return;
    }

    // Delete model members
    for (x = 0; x < geo->modelnum; x++) {
        if (geo->models[x].facenum)   free(geo->models[x].faces);
        if (geo->models[x].vertexnum) free(geo->models[x].vertices);
    }

    // Delete GEO members
    if (geo->texturenum) free(geo->textures);
    if (geo->modelnum)   free(geo->models);

    // Delete extended GEO members
    geox = (GEO_EXT *) geo;
    if (geox->data != NULL) free(geox->data);

    // Delete object and return
    free(geox);
    return;
}

// Set the verbosity level
void geoVerbose(int verbose) {
    GEO_VERBOSE = verbose;
    return;
}
