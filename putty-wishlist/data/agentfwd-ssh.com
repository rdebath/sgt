Summary: SSH-2 agent forwarding with ssh.com's product
Class: wish
Priority: medium
Difficulty: tricky
Content-type: text/x-html-body

<p>
At present, agent forwarding in SSH-2 is only available when your SSH
server is OpenSSH. The <tt>ssh.com</tt> server uses a different agent
protocol which we have not yet got round to supporting.

<p>
<b>SGT</b>: The initial IETF draft of the ssh.com agent
protocol is available here:
<a href="http://www.ietf.org/internet-drafts/draft-ietf-secsh-agent-02.txt">draft-ietf-secsh-agent-02.txt</a>
