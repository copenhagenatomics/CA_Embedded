using FFTdrawingLib;
using System.Diagnostics;
using System.IO.Ports;

namespace FFTviewer
{
    public partial class Form1 : Form
    {
        private FFTdraw _fftDraw;
        public Form1()
        {
            InitializeComponent();
            comboBox1.Items.AddRange(SerialPort.GetPortNames());
        }

        private void bitmapUpdateEvent(object sender, Bitmap bmp)
        {
            pictureBox1.Image?.Dispose();
            pictureBox1.Image = bmp;
            label1.Invoke((MethodInvoker)delegate
            {
                label1.Text = _fftDraw.Label;
            });
        }

        private void pictureBox1_SizeChanged(object sender, EventArgs e)
        {
            _fftDraw?.ChangeSize(pictureBox1.Width, pictureBox1.Height, textBox1.Text);
        }

        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            _fftDraw?.Dispose();
            _fftDraw = new FFTdraw(comboBox1.Text, pictureBox1.Width, pictureBox1.Height, textBox1.Text);
            _fftDraw.BitmapUpdateEvent += bitmapUpdateEvent;
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            _fftDraw?.Dispose();
        }

        private void pictureBox1_MouseMove(object sender, MouseEventArgs e)
        {
            label1.Text = _fftDraw?.MouseMove(e.X, e.Y);
        }

        private void numericUpDown1_ValueChanged(object sender, EventArgs e)
        {
            _fftDraw?.ChangeFilter((int)numericUpDown1.Value);
        }
    }
}