

DESCRIPTION = "Softwarebased Picture-In-Picture"
MAINTAINER = "ardi69 <ardi@ist-einmalig.de>"


LICENSE = "Proprietary"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d41d8cd98f00b204e9800998ecf8427e"

DEPENDS = "ffmpeg"
RDEPENDS_${PV} = "enigma2 libstdc++6"


PR = "alpha"
SRCREV = "${AUTOREV}"

PVBASE := "${PV}"

FILEEXTRAPATHS_prepend := "${THISDIR}/${PN}-${PVBASE}:"

PV = "${PVBASE}+svn${SRCPV}"
#PKGV = "$PV"

SRC_URI = "svn://yoda/svn/SoftwarePIP;module=trunk;protocol=http"

S = "${WORKDIR}/trunk"

PLUGINDIR = "/usr/lib/enigma2/python/Plugins/SystemPlugins/SoftwarePIP"

FILES_${PN} = "*"

do_compile() {
	${CXX} ${CXXFLAGS} -ffunction-sections -fdata-sections "-D__debugbreak()" \
		${LDFLAGS} -Wl,--gc-sections -pthread \
		-Wno-deprecated-declarations ${S}/*.cpp -l:libavcodec.a -l:libavutil.a -l:libswscale.a -l:libswresample.a -o softwarepip
	python -OO -m compileall -d ${PLUGINDIR} ${S}/plugin/
	rm -v ${S}/plugin/*.py

}

do_install() {
        install -m 0755 -d ${D}${bindir} ${D}${PLUGINDIR} ${D}${docdir}/ardi
        install -m 0755 ${S}/softwarepip ${D}${bindir}
        install -m 0644 ${S}/plugin/* ${D}${PLUGINDIR}
}
pkg_postinst_${PN}() {
#!/bin/sh -e
# Commands to carry out
echo "src/gz ardisoft http://ardisoft.de/enigma2/${DEFAULTTUNE}" > /etc/opkg/ardisoft-feed.conf
}
