# Simple SSH server for testing
FROM alpine:latest

# Install OpenSSH server
RUN apk add --no-cache openssh openssh-server-pam

# Create test user with password "test123"
RUN adduser -D testuser && \
    echo "testuser:test123" | chpasswd

# Generate SSH host keys
RUN ssh-keygen -A

# Configure SSH to allow password authentication
RUN sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config && \
    sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin no/' /etc/ssh/sshd_config

# Expose SSH port
EXPOSE 22

# Start SSH daemon
CMD ["/usr/sbin/sshd", "-D", "-e"]
