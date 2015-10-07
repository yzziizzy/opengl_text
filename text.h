




typedef struct {
	FT_Face fontFace;
	char* charSet;
	unsigned char* texture;
	unsigned char* codeIndex;
	short charLen;
	short indexLen;
	unsigned short* offsets;
	unsigned char* kerning;
	
	
} TextRes;




// this function is rather expensive
TextRes* LoadFont(char* path, int size, char* chars);

void FreeFont(TextRes* res);












