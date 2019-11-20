#include "file_utility.hpp"
#include "base.hpp"
#include "kconfig_parser_exception.hpp"

// True if this is a single byte utf8 character
inline bool isFirstByteOfSingleByte (unsigned char const c)
{
	return !(c & 0x80);
}

// True if this is the second byte of a double byte utf8 character
inline bool isSecondByteOfDoubleByte (unsigned char const c)
{
	return (c & 0xE0) == 0xC0;
}

// True if this is the third byte of a three byte utf8 character
inline bool isThirdByteOfTrippleByte (unsigned char const c)
{
	return (c & 0xF0) == 0xE0;
}

// Skips a utf8 character independent of its byte length
void skipUtf8Char (std::ifstream & input)
{
	unsigned char c = input.get ();
	if (isFirstByteOfSingleByte (c))
	{
		return;
	}
	c = input.get ();
	if (isSecondByteOfDoubleByte (c))
	{
		return;
	}
	c = input.get ();
	if (isThirdByteOfTrippleByte (c))
	{
		return;
	}
	input.get ();
}

// saves a utf8 character into output independent of its byte length
void readUtf8Char (std::ifstream & input, std::ostream & output)
{
	unsigned char c = input.get ();
	output << c;
	if (isFirstByteOfSingleByte (c))
	{
		return;
	}
	c = input.get ();
	output << c;
	if (isSecondByteOfDoubleByte (c))
	{
		return;
	}
	c = input.get ();
	output << c;
	if (isThirdByteOfTrippleByte (c))
	{
		return;
	}
	c = input.get ();
	output << c;
}

FileUtility::FileUtility (const std::string & filenameParam)
: file{ filenameParam }, stringBuffer{}, currentLine{ 1 }, filename{ filenameParam }
{
	if (!(this->file).is_open ())
	{
		throw KConfigParserException (*this, "Could not open the file.");
	}
}

char FileUtility::peekNextChar ()
{
	return (this->file).peek ();
}

bool FileUtility::isNextCharEOF ()
{
	return peekNextChar () == EOF;
}

bool FileUtility::isNextCharNewline ()
{
	char nextChar = peekNextChar ();
	return nextChar == character_newline || nextChar == character_carriage_return;
}

bool FileUtility::isNextCharNewlineOrEOF ()
{
	return isNextCharNewline () || isNextCharEOF ();
}

bool FileUtility::isNextCharToken ()
{
	switch (peekNextChar ())
	{
	case character_dollar_sign:
	case character_open_bracket:
	case character_newline:
	case character_carriage_return:
	case character_equals_sign:
	case character_hash_sign:
	case character_close_bracket:
	case EOF:
		return true;
	default:
		return false;
	}
}

void FileUtility::skipChar ()
{
	skipUtf8Char (this->file);
}

void FileUtility::skipCharsIfBlank ()
{
	while (isblank (peekNextChar ()))
	{
		skipChar ();
	}
}

void FileUtility::skipLine ()
{
	++currentLine;
	while (true)
	{
		switch ((this->file).peek ())
		{
		case character_newline:
			skipChar ();
			return;
		case character_carriage_return:
			skipChar ();
			if (peekNextChar () == character_newline) skipChar ();
			return;
		case EOF:
			return;
		default:
			skipChar ();
		}
	}
}

void FileUtility::skipLineIfEmptyOrComment ()
{
	while (true)
	{
		switch ((this->file).peek ())
		{
		case character_newline:
		case character_carriage_return:
		case character_hash_sign:
			skipLine ();
			break;
		default:
			return;
		}
	}
}

void FileUtility::readUntilChar (std::ostream & str, const char & delimiter)
{
	char c;
	while (true)
	{
		c = this->file.peek ();
		if (c == EOF || c == character_newline || c == character_carriage_return || c == delimiter)
		{
			break;
		}
		readUtf8Char (this->file, str);
	}
}

void FileUtility::readUntilChar (std::ostream & str, const char & delimiterA, const char & delimiterB)
{
	char c;
	while (true)
	{
		c = this->file.get ();
		if (c == EOF || c == character_newline || c == character_carriage_return || c == delimiterA || c == delimiterB)
		{
			this->file.putback (c);
			break;
		}
		str << c;
	}
}

std::string FileUtility::getUntilChar (const char & delimiter)
{
	// Empty the stringBuffer before re-using it
	(this->stringBuffer).str (std::string ());
	readUntilChar (this->stringBuffer, delimiter);
	return (this->stringBuffer).str ();
}

std::string FileUtility::getUntilChar (const char & delimiterA, const char & delimiterB)
{
	// Empty the stringBuffer before re-using it
	(this->stringBuffer).str (std::string ());
	readUntilChar (this->stringBuffer, delimiterA, delimiterB);
	return (this->stringBuffer).str ();
}

int FileUtility::getCurrentLineNumber () const
{
	return currentLine;
}

std::string FileUtility::getFilename () const
{
	return filename;
}
