using FFTdrawingLib;

namespace FFTviewer
{
    public partial class Form1 : Form
    {
        private FFTdraw _fftDraw = new FFTdraw();
        public Form1()
        {
            InitializeComponent();
            _fftDraw.Initialize("COM9", pictureBox1.Width, pictureBox1.Height);
            _fftDraw.BitmapUpdateEvent += bitmapUpdateEvent;
        }

        private void bitmapUpdateEvent(object sender, Bitmap bmp)
        {
            pictureBox1.Image = bmp;
        }

        private void pictureBox1_SizeChanged(object sender, EventArgs e)
        {
            _fftDraw.ChangeSize(pictureBox1.Width, pictureBox1.Height);
        }
    }
}