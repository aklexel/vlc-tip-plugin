FROM debian:stretch-slim

RUN dpkg --add-architecture i386 && \
	apt-get update && apt-get install -y \
		gcc-multilib \
		make \
		pkg-config \
		libvlc-dev:i386 libvlccore-dev:i386 \
		libvlc-dev:amd64 libvlccore-dev:amd64 \
		gcc-mingw-w64-i686 \
        gcc-mingw-w64-x86-64 \
		wget \
		p7zip \
	&& wget ftp://ftp.videolan.org/pub/videolan/vlc/last/win32/vlc-*-win32.7z \
	&& wget ftp://ftp.videolan.org/pub/videolan/vlc/last/win64/vlc-*-win64.7z \
	&& 7zr x "*.7z" -o"/opt/*" */sdk \
	&& rm *.7z \
	&& rm -rf /var/lib/apt/lists/*

ADD build.sh .

ENTRYPOINT ["/bin/bash", "build.sh"]