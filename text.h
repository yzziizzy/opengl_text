
#include <ft2build.h>
#include FT_FREETYPE_H



typedef struct {
	FT_Face fontFace;
	char* charSet;
	unsigned char* texture;
	unsigned char* codeIndex;
	short charLen;
	short indexLen;
	unsigned short* offsets;
	unsigned char* kerning;
	
	GLuint textureID;
	short texWidth;
	short texHeight;
	
} TextRes;


typedef struct {
	float x,y,z;
	float u,v;
} TextVertex;

typedef struct {
	TextVertex* vertices;
	int vertexCnt;
	
	GLuint vao;
	GLuint vbo;
	
	TextRes* font;
	
} TextRenderInfo;



// this function is rather expensive. it rebinds textures.
TextRes* LoadFont(char* path, int size, char* chars);

void drawText(TextRes* font, char* str, int len, Matrix* m);

void FreeFont(TextRes* res);












