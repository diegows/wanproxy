#ifndef	SSH_SERVER_HOST_KEY_H
#define	SSH_SERVER_HOST_KEY_H

class SSHServerHostKey {
	std::string name_;
protected:
	SSHServerHostKey(const std::string& xname)
	: name_(xname)
	{ }

public:
	~SSHServerHostKey()
	{ }

	std::string name(void) const
	{
		return (name_);
	}

	virtual bool input(Buffer *) = 0;
};

#endif /* !SSH_SERVER_HOST_KEY_H */
