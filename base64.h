int encode_base64(unsigned char const *src, unsigned int srclength, 
		char *target, unsigned int targsize);
int decode_base64(char const *src, unsigned char *target, 
		unsigned int targsize);
