Summary: SSH-2 agent forwarding with ssh.com's product
Class: wish
Priority: medium
Difficulty: taxing
Content-type: text/x-html-body

<p>
At present, agent forwarding in SSH-2 is only available when your SSH
server is OpenSSH. The <tt>ssh.com</tt> server uses a different agent
protocol which they have not published. If you would like PuTTY to be
able to support agent forwarding to an <tt>ssh.com</tt> server, please
write to <tt>ssh.com</tt> and explain to them that they are hurting
themselves and their users by keeping their protocol secret.
