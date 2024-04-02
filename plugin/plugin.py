
from os import path

from struct import unpack
from subprocess import Popen, PIPE, STDOUT
from threading import Thread, Lock, Event
from time import sleep

from Screens.Screen import Screen
from Components.Label import Label
from Components.Pixmap import Pixmap
from Components.AVSwitch import AVSwitch
from Components.ActionMap import ActionMap
from Components.config import config
from Plugins.Plugin import PluginDescriptor
from enigma import ePicLoad

###########################################################################

class SoftwarePIP(object):
	def __init__(self, pixmap, uri):
		self.pixmap = pixmap
		self.uri = uri
		# self.pixmap.instance.setPixmapFromFile("/usr/share/enigma2/skin_default/icons/dish.png")

		self.Scale = AVSwitch().getFramebufferScale()
		self.PicLoadPara = [self.pixmap.instance.size().width(), self.pixmap.instance.size().height(), self.Scale[0], self.Scale[1], 0, 1, "#002C2C39"]
		self.PicLoad = ePicLoad()
		self.PicLoad.setPara(self.PicLoadPara)
		self.PicLoad.PictureData.get().append(self.showFrame)
		self.lock = Lock()
		self.event = Event()
		self.curr_pic = None
		self.next_pic = None
		self.thread = Thread(target=self.runThread)
		self.stopped = False

	def __del__(self):
		print "[SoftwarePIP] __del__"

	def start(self):
		print path.abspath(__file__) + "/SoftwarePIP.png"
		self.PicLoad.startDecode(path.dirname(path.abspath(__file__)) + "/SoftwarePIP.png")
		self.thread.start()
		#self.PicLoad.startDecode("/usr/share/enigma2/skin_default/icons/dish.png")

	def decodeCurr(self):
		if self.curr_pic is None: return
		with open("/tmp/SoftwarePIP.bmp", "wb") as file:
			file.write(self.curr_pic['header'])
			file.write(self.curr_pic['image'])
			file.close()
			self.PicLoad.startDecode("/tmp/SoftwarePIP.bmp")

	def decodeNext(self):
		curr = None
		with self.lock:
			curr = self.curr_pic = self.next_pic
			self.next_pic = None
		if curr is not None:
			self.decodeCurr()

	def setNext(self, next):
		curr = None
		with self.lock:
			if self.curr_pic is None:
				curr = self.curr_pic = next
				self.next_pic = None
			else:
				if self.next_pic is not None: print "[SoftwarePIP] skip Picture"
				self.next_pic = next
		if curr is not None:
			self.decodeCurr()

	def showFrame(self, PicInfo = ""):
		if not self.stopped:
			self.pixmap.instance.setPixmap(self.PicLoad.getData())
			self.decodeNext()

	def stop(self):
		print "[SoftwarePIP] stop"
		self.stopped = True
		self.event.set()
		self.thread.join()

	def runThread(self):
		pipe = Popen(["softwarepip", '-i', self.uri], stdin=PIPE, stdout=PIPE, bufsize=10**6) # , env=FFMPEG_ENV

		def getInteger(buffer, index):
			return unpack("<I", buffer[index:index+4])[0]

		while not self.event.isSet():
			pic = {};
			pic['header'] = pipe.stdout.read(14+40)
			if len(pic['header']) != 54:
				break
			filesize = getInteger(pic['header'], 2);
			pic['image'] = pipe.stdout.read(filesize-(14+40))
			if len(pic['image']) != filesize-(14+40):
				break;
			pic['width'] = getInteger(pic['header'], 18)
			pic['height'] = getInteger(pic['header'], 22)
			self.setNext(pic)
		pipe.terminate()
		pipe.stdout.close()
		pipe.stdin.close()
		print "[SoftwarePIP] thread finished"



###########################################################################


### LD_LIBRARY_PATH=/usr/lib/enigma2/python/Plugins/Extensions/ardi/bin ffmpeg -i "http://127.0.0.1:8001/1:0:1:445D:453:1:C00000:0:0:0:" -an -vf "select=eq(pict_type\,I)+eq(pict_type\,P),scale=320:-1" -vsync vfr -f image2pipe -codec bmp - > /dev/null
###########################################################################

old_PictureInPicture_init = None
def hooked_PictureInPicture_init(self, session):
	print("pre hooked_PictureInPicture_init")
	old_PictureInPicture_init(self, session)
	self["video"] = Pixmap();
	#self["video"].instance.setPixmapFromFile("/usr/share/enigma2/skin_default/icons/dish.png");
	self.playing = False
	print("post hooked_PictureInPicture_init")


old_PictureInPicture_del = None
def hooked_PictureInPicture_del(self):
	print("pre hooked_PictureInPicture_del")
	if hasattr(self, "pipservice"):
	   if self.pipservice is not None: self.pipservice.stop()
	else:
		# Screens.PictureInPicture..PictureInPicture.__del__ dont check hasattr(self, "pipservice")
		self.pipservice = None
	old_PictureInPicture_del(self)
	print("post hooked_PictureInPicture_del")


old_PictureInPicture_setSizePosMainWindow = None
def hooked_PictureInPicture_setSizePosMainWindow(self, x = 0, y = 0, w = 0, h = 0):
	pass

old_PictureInPicture_playService = None
def hooked_PictureInPicture_playService(self, service):
	if service is None:
		return False
	ref = self.resolveAlternatePipService(service)
	if ref:
#		var = "play http://127.0.0.1:8001/" + ref.toString()
#		print "##############################################" + var
		if self.isPlayableForPipService(ref):
			print "playing pip service", ref and ref.toString()
		else:
			if not config.usage.hide_zap_errors.value:
				Notifications.AddPopup(text = _("No free tuner!"), type = MessageBox.TYPE_ERROR, timeout = 5, id = "ZapPipError")
			return False
		if hasattr(self, "pipservice") and self.pipservice is not None: self.pipservice.stop()
		self.pipservice = SoftwarePIP(self["video"], "http://127.0.0.1:8001/" + ref.toString()) # eServiceCenter.getInstance().play(ref)
		if hasattr(self, "dishpipActive") and self.dishpipActive is not None:
			self.dishpipActive.startPiPService(ref)
		self.pipservice.start()
		self.currentService = service
		self.currentServiceReference = ref
		return True
	return False


def autostart(reason, **kwargs):

	from Components.SystemInfo import SystemInfo
	from Screens.InfoBar import InfoBar
	global old_PictureInPicture_init
	if reason == 0:
		#print "error: ****************************************************************"
		#print "error: ****************************************************************"
		#print "error: *********SoftwarePIP Start *************************************"
		#print "error: ****************************************************************"
		#print "error: ****************************************************************"

		if SystemInfo["PIPAvailable"]: return	# have HW-PiP
		SystemInfo["PIPAvailable"] = True		# enable PIP
		if InfoBar.instance is not None:
		#	print "InfoBar.instance != None"
			instance = InfoBar.instance
			instance.addExtension((instance.getShowHideName, instance.showPiP, lambda: True), "blue")
			instance.addExtension((instance.getMoveName, instance.movePiP, instance.pipShown), "green")
			instance.addExtension((instance.getSwapName, instance.swapPiP, instance.pipShown), "yellow")
			instance.addExtension((instance.getTogglePipzapName, instance.togglePipzap, instance.pipShown), "red")
			instance.updateExtensions()
		#else:
		#	print "InfoBar.instance == None"
	else:
		#print "error: ****************************************************************"
		#print "error: ****************************************************************"
		#print "error: *********SoftwarePIP Stop **************************************"
		#print "error: ****************************************************************"
		#print "error: ****************************************************************"

		if old_PictureInPicture_init is None:
			return								# not inited
		SystemInfo["PIPAvailable"] = False		# disable PIP
		if InfoBar.instance is not None:
		#	print "InfoBar.instance != None"
			instance = InfoBar.instance
			instance.list = [item for item in instance.list if item[0] == 1 or (item[1][1] != instance.showPiP and item[1][1] != instance.movePiP and item[1][1] != instance.swapPiP and item[1][1] != instance.togglePipzap)]
			instance.updateExtensions()
		#else:
		#	print "InfoBar.instance == None"

	from enigma import getDesktop
	from Screens import PictureInPicture, PiPSetup
	MAX_X = getDesktop(0).size().width()
	if MAX_X and MAX_X > 720:
		MAX_Y = getDesktop(0).size().height()
		PictureInPicture.MAX_X = PiPSetup.MAX_X = MAX_X
		PictureInPicture.MAX_Y = PiPSetup.MAX_Y = MAX_Y
		PiPSetup.MAX_W = MAX_X * 3 / 4
		PiPSetup.MAX_H = MAX_Y * 3 / 4
		PiPSetup.MIN_W = MAX_X / 8
		PiPSetup.MIN_H = MAX_Y / 8


	### hook PictureInPicture.__init__
	#global old_PictureInPicture_init
	if reason == 0:
		old_PictureInPicture_init = PictureInPicture.PictureInPicture.__init__
		PictureInPicture.PictureInPicture.__init__ = hooked_PictureInPicture_init
	else:
		PictureInPicture.PictureInPicture.__init__ = old_PictureInPicture_init
		old_PictureInPicture_init = None

	### hook PictureInPicture.__del__
	global old_PictureInPicture_del
	if reason == 0:
		old_PictureInPicture_del = PictureInPicture.PictureInPicture.__del__
		PictureInPicture.PictureInPicture.__del__ = hooked_PictureInPicture_del
	else:
		PictureInPicture.PictureInPicture.__del__ = old_PictureInPicture_del
		old_PictureInPicture_del = None

	### hook PictureInPicture.setSizePosMainWindow
	global old_PictureInPicture_setSizePosMainWindow
	if reason == 0:
		old_PictureInPicture_setSizePosMainWindow = PictureInPicture.PictureInPicture.setSizePosMainWindow
		PictureInPicture.PictureInPicture.setSizePosMainWindow = hooked_PictureInPicture_setSizePosMainWindow
	else:
		PictureInPicture.PictureInPicture.setSizePosMainWindow = old_PictureInPicture_setSizePosMainWindow
		old_PictureInPicture_setSizePosMainWindow = None

	### hook PictureInPicture.playService
	global old_PictureInPicture_playService
	if reason == 0:
		old_PictureInPicture_playService = PictureInPicture.PictureInPicture.playService
		PictureInPicture.PictureInPicture.playService = hooked_PictureInPicture_playService
	else:
		PictureInPicture.PictureInPicture.playService = old_PictureInPicture_playService
		old_PictureInPicture_playService = None

###########################################################################

def Plugins(**kwargs): return [
		PluginDescriptor(where = PluginDescriptor.WHERE_AUTOSTART, fnc=autostart)
	]
