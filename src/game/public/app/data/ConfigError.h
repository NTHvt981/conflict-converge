#pragma once

// Failure cause for the config loaders in app/data. Files under data/configs/
// are hand-editable, so a rejected file names exactly one of these.

enum class ConfigError
{
	FileUnreadable,    // missing or unreadable file
	FileUnwritable,    // save target could not be written
	MalformedJson,     // cereal rejected the document
	UnknownName,       // a name that matches no enumerator
	Mismatch,          // file stem disagrees with the declared type
	UnknownAbilityKey, // abilities block carries a key nothing reads
	InvalidData,       // parsed, but the content is inconsistent
};

inline const char *Describe(ConfigError error)
{
	switch (error)
	{
	case ConfigError::FileUnreadable:
		return "file could not be read";
	case ConfigError::FileUnwritable:
		return "file could not be written";
	case ConfigError::MalformedJson:
		return "malformed JSON";
	case ConfigError::UnknownName:
		return "unknown name";
	case ConfigError::Mismatch:
		return "file stem does not match the declared type";
	case ConfigError::UnknownAbilityKey:
		return "unknown ability key";
	case ConfigError::InvalidData:
		return "inconsistent data";
	}
	return "unknown error";
}
