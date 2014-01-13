#ifndef __COH_GEO__
#define __COH_GEO__

typedef struct {
    int v1, v2, v3;
    int texture;
} GEO_FACE;

typedef struct {
    float  x,  y,  z;
    float nx, ny, nz;
    float  s,  t;
} GEO_VERTEX;

typedef struct {
    char       *id;
    int         facenum;
    GEO_FACE   *faces;
    int         vertexnum;
    GEO_VERTEX *vertices;
} GEO_MODEL;

typedef struct {
    char      *id;
    int        texturenum;
    char     **textures;
    int        modelnum;
    GEO_MODEL *models;
} GEO;

GEO* geoLoad(unsigned char *, int);
void geoFree(GEO *);
void geoVerbose(int);

#endif // __GOH_GEO__
