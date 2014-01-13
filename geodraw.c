#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tpkapi.h"
#include "geo.h"

// Macros to take the place of common functions
#define GetInt16(x, y) ( \
    ((int) x[y] <<  8) | ((int) x[y + 1]) )
#define GetInt32(x, y) ( \
    ((int) x[y] << 24) | ((int) x[y + 1] << 16) | \
    ((int) x[y + 2] <<  8) | ((int) x[y + 3]) )

typedef __stdcall int (*ZL_UNC)(unsigned char *, int *, unsigned char *, int);

HMODULE hZlib = NULL;
ZL_UNC huncompress = NULL;
TPK_WINDOW *hWnd;
TPK_GLRC   *hRC;
unsigned int lastms, model = 0, *textures;
float xrot = 0.0f, yrot = 0.0f, zrot = 0.0f;
float cx, cy, cz, scale;
GEO_MODEL *mod;
int rot[10] = {0, 0, 0, 0, 0, 0, 0, 0};
float xsft = 0.0f, ysft = 0.0f, zsft = 0.0f;

int uncompress(void *dest, int *destlen, void *src, int srclen) {
    if (huncompress == NULL) return 1;
    return huncompress(dest, destlen, src, srclen);
}

int CheckArgs(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <geofile>\n", argv[0]);
        return 1;
    }

    return 0;
}

int InitZlib() {
    hZlib = LoadLibrary("zlib1.dll");
    if (hZlib == NULL) {
        printf("ERROR: Could not load zlib1.dll\n");
        return 2;
    }

    huncompress = (ZL_UNC) GetProcAddress(hZlib, "uncompress");
    if (huncompress == NULL) {
        printf("ERROR: Could not locate uncompress()\n");
        FreeLibrary(hZlib);
        return 3;
    }

    return 0;
}

int LoadFile(char *filename, unsigned char **buffer) {
    unsigned char *fData;
    FILE *fPtr;
    int fLen, err;

    fPtr = fopen(filename, "rb");
    if (fPtr == NULL) { *buffer = NULL; return 0; }

    fseek(fPtr, 0, SEEK_END);
    fLen = ftell(fPtr);
    if (fLen <= 0) { fclose(fPtr); return 0; }
    fseek(fPtr, 0, SEEK_SET);

    fData = malloc(fLen);
    err = fread(fData, 1, fLen, fPtr);
    fclose(fPtr);
    if (err != fLen) { free(fData); return 0; }

    *buffer = fData;
    return fLen;
}

void Breakdown(GEO *geo) {
    if (geo != NULL) geoFree(geo);
    FreeLibrary(hZlib);
    return;
}















// Alternative to gluPerspective()
void gluPerspective(double fovy, double aspect, double zNear, double zFar) {
    double xMin, xMax, yMin, yMax;

    // Calculate frustum bounds
    yMax = zNear * tan(fovy * 0.0087266463);
    yMin = -yMax;
    xMin = yMin * aspect;
    xMax = -xMin;

    // Apply and exit
    glFrustum(xMin, xMax, yMin, yMax, zNear, zFar);
    return;
}

// Configures the OpenGL viewport given dimensions
void configviewport(int width, int height) {
    double aspect;

    // Calculate aspect ratio
    width  = (width  < 1) ? 1 : width;
    height = (height < 1) ? 1 : height;
    aspect = (double) width / (double) height;

    // Adjust viewport settings
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0, aspect, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
    return;
}

// Prepare the program for doing its thing
int initialize() {
    float param[4];

    // Start up the API
    if (tpkStartup() != TPK_ERR_NONE) {
        printf("Error starting up the API\n");
        return 1;
    }

    // Make a window
    hWnd = tpkCreateWindow(640, 480, "GeoDraw");
    if (hWnd == NULL) {
        tpkShutdown();
        printf("Could not create window.\n");
        return 1;
    }

    // Make an OpenGL rendering context
    hRC = tpkCreateGLRC(hWnd);
    if (hRC == NULL) {
        tpkDelete(hWnd);
        tpkShutdown();
        printf("Could not create OpenGL rendering context.\n");
        return 1;
    }

    // Configuring OpenGL parameters
    tpkMakeCurrent(hRC);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    configviewport(hWnd->width, hWnd->height);

    glCullFace(GL_FRONT);
    //glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Configuring Light 0
    param[0] = 1.0f; param[1] = 1.0f; param[2] = 1.0f; param[3] = 1.0f;
    glLightfv(GL_LIGHT0, GL_AMBIENT, param);
    param[0] = 1.0f; param[1] = 1.0f; param[2] = 1.0f; param[3] = 1.0f;
    glLightfv(GL_LIGHT0, GL_DIFFUSE, param);
    param[0] = 0.0f; param[1] = 0.0f; param[2] = 0.0f; param[3] = 1.0f;
    glLightfv(GL_LIGHT0, GL_POSITION, param);

    srand(tpkTimer(&lastms));
    return 0;
}

// Pack up the program for exiting
void uninitialize() {
    tpkDelete(hWnd);
    tpkShutdown();
    return;
}


// Loads a model
void LoadModel(GEO *geo) {
    float maxx, maxy, maxz, minx, miny, minz, dist;
    int x, y; GEO_VERTEX *v;

    mod = &geo->models[model];
    sprintf(hWnd->text, "%d %s", model, mod->id);
    tpkUpdate(hWnd);

    v = &mod->vertices[mod->faces[0].v1];
    maxx = minx = v->x;
    maxy = miny = v->y;
    maxz = minz = v->z;

    for (x = 1; x < mod->facenum; x++) {
        for (y = 0; y < 3; y++) {
            if (y == 0) v = &mod->vertices[mod->faces[x].v1];
            if (y == 1) v = &mod->vertices[mod->faces[x].v2];
            if (y == 2) v = &mod->vertices[mod->faces[x].v3];

            if (v->x < minx) minx = v->x;
            if (v->x > maxx) maxx = v->x;
            if (v->y < miny) miny = v->y;
            if (v->y > maxy) maxy = v->y;
            if (v->z < minz) minz = v->z;
            if (v->z > maxz) maxz = v->z;
        }
    }

    cx = minx + (maxx - minx) / 2;
    cy = miny + (maxy - miny) / 2;
    cz = minz + (maxz - minz) / 2;

    maxx -= minx; maxy -= miny; maxz -= minz;
    dist = maxx;
    if (maxy > dist) dist = maxy;
    if (maxz > dist) dist = maxz;
    scale = 10.0f / dist;

    xrot = yrot = zrot = xsft = ysft = zsft = 0.0f;

    return;
}


// Process window events
int events(GEO *geo) {
    int arg1, arg2, event, closing = 0, old = model;

    // Read all supported events
    event = TPK_EVENT_NONE;
    do {
        event = tpkNextEvent(hWnd, &arg1, &arg2);
        switch (event) {
        case TPK_EVENT_CLOSE:
            closing = 1;
            break;
        case TPK_EVENT_RESIZE:
            configviewport(hWnd->width, hWnd->height);
            break;
        case TPK_EVENT_KEYUP:
            if (arg1 == 37) rot[0] = 0;
            if (arg1 == 39) rot[1] = 0;
            if (arg1 == 38) rot[2] = 0;
            if (arg1 == 40) rot[3] = 0;
            if (arg1 == 46) rot[4] = 0;
            if (arg1 == 34) rot[5] = 0;
            if (arg1 == 36) rot[6] = 0;
            if (arg1 == 35) rot[7] = 0;
            if (arg1 == 45) rot[8] = 0;
            if (arg1 == 33) rot[9] = 0;
            break;
        case TPK_EVENT_KEYDOWN:
            if (arg1 == 37) rot[0] = 1;
            if (arg1 == 39) rot[1] = 1;
            if (arg1 == 38) rot[2] = 1;
            if (arg1 == 40) rot[3] = 1;
            if (arg1 == 46) rot[4] = 1;
            if (arg1 == 34) rot[5] = 1;
            if (arg1 == 36) rot[6] = 1;
            if (arg1 == 35) rot[7] = 1;
            if (arg1 == 45) rot[8] = 1;
            if (arg1 == 33) rot[9] = 1;

            if (arg1 == 32) model++;
            if (arg1 ==  8) model--;
            if (model == -1) model += geo->modelnum;
            if (model == geo->modelnum) model = 0;
            if (model != old) LoadModel(geo);
            break;
        default: break;
        }

    } while (event != TPK_EVENT_NONE);

    // Skip remaining events
    tpkDoEvents();
    return closing;
}

// Animate one frame's worth
void animate() {
    if (rot[0]) yrot -= 1.0f;
    if (rot[1]) yrot += 1.0f;
    if (rot[2]) xrot -= 1.0f;
    if (rot[3]) xrot += 1.0f;
    if (yrot < 0.0f) yrot += 360.0f;
    if (yrot > 360.0f) yrot -= 360.0f;
    if (xrot < -90.0f) xrot = -90.0;
    if (xrot > 90.0f) xrot = 90.0f;

    if (rot[4]) xsft -= 0.03f;
    if (rot[5]) xsft += 0.03f;
    if (rot[6]) ysft += 0.03f;
    if (rot[7]) ysft -= 0.03f;
    if (rot[8]) zsft -= 0.05f;
    if (rot[9]) zsft += 0.05f;

    return;
}

// Draw the OpenGL scene
void drawscene() {
    GEO_VERTEX *v;
    int x, pass;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);

    glPushMatrix();
        glTranslatef(xsft, ysft, -15.0f + zsft);
        glRotatef(xrot, 1.0f, 0.0f, 0.0f);
        glRotatef(yrot, 0.0f, 1.0f, 0.0f);

        glScalef(-scale, scale, scale);
        glTranslatef(-cx, -cy, -cz);

        //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        for (x = 0; x < mod->facenum; x++) {
            glBindTexture(GL_TEXTURE_2D, textures[mod->faces[x].texture]);

            glBegin(GL_TRIANGLES);
            v = &mod->vertices[mod->faces[x].v1];
            glNormal3f(v->nx * scale, v->ny * scale, v->nz * scale);
            glTexCoord2f(v->s, v->t);
            glVertex3f(v->x, v->y, v->z);
            v = &mod->vertices[mod->faces[x].v2];
            glNormal3f(v->nx * scale, v->ny * scale, v->nz * scale);
            glTexCoord2f(v->s, v->t);
            glVertex3f(v->x, v->y, v->z);
            v = &mod->vertices[mod->faces[x].v3];
            glNormal3f(v->nx * scale, v->ny * scale, v->nz * scale);
            glTexCoord2f(v->s, v->t);
            glVertex3f(v->x, v->y, v->z);
            glEnd();
        }

    glPopMatrix();

    glFinish();
    tpkSwapBuffers(hRC);
    return;
}

// Main program loop
void prgloop(GEO *geo) {
    double target = 1000.0 / 120.0; // Number of milliseconds per frame
    double accum = 0.0;             // Milliseconds accumulated
    int closing = 0;

    // Loop until program exit is requested
    while (!closing) {

        // Wait until at least 1 frame elapses
        while (accum < 1.0) {
            accum += ((double) tpkTimer(&lastms) / target);
            if (accum < 1.0) tpkSleep(1); // Give the CPU some slack
        }

        // Perform control operations for any skipped frames
        for ( ; accum >= 1.0; accum -= 1.0)
            animate();

        // Draw one frame
        drawscene(geo);

        // Process window events
        closing = events(geo);
    }
    
}

void LoadTexture(char *filename, int dest) {
    char fname[256];
    unsigned char *fData, *pData;
    int fLen, width, height, depth, clen, plen, fOff, ulen;

    glBindTexture(GL_TEXTURE_2D, textures[dest]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    sprintf(fname, "textures\\%s", filename);
    fLen = strlen(fname);
    if (fname[fLen - 4] != '.') strcat(fname, ".png");
    strcpy(&fname[strlen(fname) - 3], "png");

    fLen = LoadFile(fname, &fData);
    if (fData == NULL) return;

    width = GetInt32(fData, 0x10);
    height = GetInt32(fData, 0x14);
    fOff = 0x21;
    plen = 0;
    while (fOff < fLen) {
        clen = GetInt32(fData, fOff); fOff += 4;

        if (!strncmp(&fData[fOff], "IDAT", 4)) {
            fOff += 4;
            if (clen) memmove(&fData[plen], &fData[fOff], clen);
            plen += clen;
        } else fOff += 4;

        fOff += clen + 4;
    }

    ulen = width * height * 4 + height * 2;
    pData = malloc(ulen);
    fLen = uncompress(pData, &ulen, fData, plen);
    free(fData);

    if (fLen) { free(pData); return; }

    fData = malloc(ulen);

    for (fLen = clen = 0; fLen < height; fLen++) {
        ulen = height - fLen - 1;
        memmove(&fData[clen], 
            &pData[ulen * (width * 4 + 1) + 1], 
            width * 4);
        clen += width * 4;
    }
    free(pData);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, 
        GL_RGBA, GL_UNSIGNED_BYTE, fData);
    free(fData);

    return;
}

int main(int argc, char **argv) {
    unsigned char *fData;
    GEO *geo = NULL;
    int err, fLen, x;

    err = CheckArgs(argc, argv); if (err) return err;
    err = InitZlib();            if (err) return err;
    geoVerbose(1);

    fLen = LoadFile(argv[1], &fData);
    if (!fLen) {
        printf("ERROR: Could not load %s\n", argv[1]);
        return 4;
    }

    geo = geoLoad(fData, fLen);
    free(fData);
    if (geo == NULL) {
        Breakdown(geo);
        return 5;
    }

    printf("Loaded %s\n", argv[1]);
    printf("ID = %s\n", geo->id);

    printf("\nTextures: %d\n", geo->texturenum);
    for (x = 0; x < geo->texturenum; x++)
        printf("  %3d  %s\n", x, geo->textures[x]);

    printf("\nModels: %d\n", geo->modelnum);
    for (x = 0; x < geo->modelnum; x++)
        printf("  %3d  %s\n", x, geo->models[x].id);



    if (initialize()) { Breakdown(geo); return 1; }
    textures = malloc(geo->texturenum * sizeof(int));
    glGenTextures(geo->texturenum, textures);
    for (x = 0; x < geo->texturenum; x++)
        LoadTexture(geo->textures[x], x);

    LoadModel(geo);
    prgloop(geo);

    glDeleteTextures(geo->texturenum, textures);
    free(textures);

    uninitialize();
    Breakdown(geo);

    return 0;
}
