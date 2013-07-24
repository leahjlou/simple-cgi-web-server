#include <vector>
#include "server.h"

#define MAX_MSG_SZ	1024

// Determine if the character is whitespace
bool isWhitespace(char c) {
	switch (c) {
	case '\r':
	case '\n':
	case ' ':
	case '\0':
		return true;
	default:
		return false;
	}
}

// Strip off whitespace characters from the end of the line
void chomp(char *line) {
	int len = strlen(line);
	while (isWhitespace(line[len])) {
		line[len--] = '\0';
	}
}

// Read the line one character at a time, looking for the CR
// You dont want to read too far, or you will mess up the content
char * GetLine(int fds) {
	char tline[MAX_MSG_SZ];
	char *line;

	int messagesize = 0;
	int amtread = 0;
	while ((amtread = read(fds, tline + messagesize, 1)) < MAX_MSG_SZ) {
		if (amtread > 0){
			messagesize += amtread;
		} /*else if(amtread == 0){
			
		}*/
		else {
			perror("Socket Error is:");
			fprintf(stderr,
				"Read Failed on file descriptor %d messagesize = %d\n", fds,
				messagesize);
			//exit(2);
			return "";
		}
		//fprintf(stderr,"%d[%c]", messagesize,message[messagesize-1]);
		if (tline[messagesize - 1] == '\n')
			break;
	}
	tline[messagesize] = '\0';
	chomp(tline);
	line = (char *) malloc((strlen(tline) + 1) * sizeof(char));
	strcpy(line, tline);
	//fprintf(stderr, "GetLine: [%s]\n", line);
	return line;
}

// Change to upper case and replace with underlines for CGI scripts
void UpcaseAndReplaceDashWithUnderline(char *str) {
	int i;
	char *s;

	s = str;
	for (i = 0; s[i] != ':'; i++) {
		if (s[i] >= 'a' && s[i] <= 'z')
			s[i] = 'A' + (s[i] - 'a');

		if (s[i] == '-')
			s[i] = '_';
	}
}

// When calling CGI scripts, you will have to convert header strings
// before inserting them into the environment.  This routine does most
// of the conversion
char *FormatHeader(char *str, char *prefix) {
	char *result = (char *) malloc(strlen(str) + strlen(prefix));
	char* value = strchr(str, ':') + 2;
	UpcaseAndReplaceDashWithUnderline(str);
	*(strchr(str, ':')) = '\0';
	sprintf(result, "%s%s=%s", prefix, str, value);
	return result;
}
