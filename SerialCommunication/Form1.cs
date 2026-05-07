using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace SerialCommunication
{
    public partial class Form1 : Form
    {
        private System.IO.Ports.SerialPort serialPortArduino = new System.IO.Ports.SerialPort();
        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            try
            {
                string[] portNames = SerialPort.GetPortNames().Distinct().ToArray();
                comboBoxPoort.Items.Clear();
                comboBoxPoort.Items.AddRange(portNames);
                if (comboBoxPoort.Items.Count > 0) comboBoxPoort.SelectedIndex = 0;

                comboBoxBaudrate.SelectedIndex = comboBoxBaudrate.Items.IndexOf("115200");
            }
            catch (Exception)
            { }
        }

        private void cboPoort_DropDown(object sender, EventArgs e)
        {
            try
            {
                string selected = (string)comboBoxPoort.SelectedItem;
                string[] portNames = SerialPort.GetPortNames().Distinct().ToArray();

                comboBoxPoort.Items.Clear();
                comboBoxPoort.Items.AddRange(portNames);

                comboBoxPoort.SelectedIndex = comboBoxPoort.Items.IndexOf(selected);
            }
            catch (Exception)
            {
                if (comboBoxPoort.Items.Count > 0) comboBoxPoort.SelectedIndex = 0;
            }
        }

        private void buttonConnect_Click(object sender, EventArgs e)
        {
            try
            {
                if (serialPortArduino.IsOpen)
                {
                    // Disconnect
                    serialPortArduino.Close();
                    radioButtonVerbonden.Checked = false;
                    buttonConnect.Text = "Connect";
                    labelStatus.Text = "Disconnected";
                }
                else
                {
                    // Connect
                    if (comboBoxPoort.SelectedItem == null)
                    {
                        MessageBox.Show("Select a COM port first.", "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        return;
                    }

                    serialPortArduino.PortName = comboBoxPoort.SelectedItem.ToString();
                    serialPortArduino.BaudRate = int.Parse(comboBoxBaudrate.SelectedItem.ToString());
                    serialPortArduino.DataBits = (int)numericUpDownDatabits.Value;

                    // Parity
                    if (radioButtonParityNone.Checked) serialPortArduino.Parity = Parity.None;
                    else if (radioButtonParityEven.Checked) serialPortArduino.Parity = Parity.Even;
                    else if (radioButtonParityOdd.Checked) serialPortArduino.Parity = Parity.Odd;
                    else if (radioButtonParityMark.Checked) serialPortArduino.Parity = Parity.Mark;
                    else if (radioButtonParitySpace.Checked) serialPortArduino.Parity = Parity.Space;

                    // StopBits
                    if (radioButtonStopbitsNone.Checked) serialPortArduino.StopBits = StopBits.None;
                    else if (radioButtonStopbitsOne.Checked) serialPortArduino.StopBits = StopBits.One;
                    else if (radioButtonStopbitsOnePointFive.Checked) serialPortArduino.StopBits = StopBits.OnePointFive;
                    else if (radioButtonStopbitsTwo.Checked) serialPortArduino.StopBits = StopBits.Two;

                    // Handshake
                    if (radioButtonHandshakeNone.Checked) serialPortArduino.Handshake = Handshake.None;
                    else if (radioButtonHandshakeRTS.Checked) serialPortArduino.Handshake = Handshake.RequestToSend;
                    else if (radioButtonHandshakeRTSXonXoff.Checked) serialPortArduino.Handshake = Handshake.RequestToSendXOnXOff;
                    else if (radioButtonHandshakeXonXoff.Checked) serialPortArduino.Handshake = Handshake.XOnXOff;

                    serialPortArduino.RtsEnable = checkBoxRtsEnable.Checked;
                    serialPortArduino.DtrEnable = checkBoxDtrEnable.Checked;
                    serialPortArduino.ReadTimeout = 3000;
                    serialPortArduino.WriteTimeout = 3000;

                    serialPortArduino.Open();


                    // ping/pong check
                    serialPortArduino.DiscardInBuffer();
                    serialPortArduino.WriteLine("ping");
                    serialPortArduino.ReadTimeout = 3000;
                    string reply = serialPortArduino.ReadLine().Trim();
                    if (reply == "pong")
                    {
                        radioButtonVerbonden.Checked = true;
                        buttonConnect.Text = "Disconnect";
                        labelStatus.Text = $"Connected to {serialPortArduino.PortName}";
                    }
                    else
                    {
                        labelStatus.Text = "Unexpected reply: " + reply;
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                labelStatus.Text = "Error: " + ex.Message;
            }
        }

        private void tabControl_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (tabControl.SelectedTab == tabPageOefening5)
            {
                timerOefening5.Start();
            }
            else
            {
                timerOefening5.Stop();
            }
        }

        private void timerOefening5_Tick(object sender, EventArgs e)
        {
            if (!serialPortArduino.IsOpen)
            {
                labelStatus.Text = "Not connected";
                radioButtonVerbonden.Checked = false;
                buttonConnect.Text = "Connect";
                timerOefening5.Stop();
                return;
            }

            try
            {
                serialPortArduino.DiscardInBuffer();

                // Read desired temperature from potentiometer (A0)
                serialPortArduino.WriteLine("get a0");
                string reply0 = serialPortArduino.ReadLine().Trim();
                int raw0 = ParseAnalogReply(reply0);
                double desired = raw0 * (40.0 / 1023.0) + 5.0; // scale 0..1023 -> 5..45

                // Read current temperature from LM35 (A1)
                serialPortArduino.WriteLine("get a1");
                string reply1 = serialPortArduino.ReadLine().Trim();
                int raw1 = ParseAnalogReply(reply1);
                double current = raw1 * (500.0 / 1023.0); // scale 0..1023 -> 0..500 (LM35)

                labelGewensteTemp.Text = desired.ToString("0.0") + " °C";
                labelHuidigeTemp.Text = current.ToString("0.0") + " °C";

                // Turn LED on pin 2 on when current < desired
                if (current < desired)
                {
                    serialPortArduino.WriteLine("set d2 1");
                }
                else
                {
                    serialPortArduino.WriteLine("set d2 0");
                }

                labelStatus.Text = $"Last update: {DateTime.Now:T}";
            }
            catch (TimeoutException)
            {
                labelStatus.Text = "Timeout reading from Arduino";
            }
            catch (System.IO.IOException ioex)
            {
                HandleDisconnect("IO Error: " + ioex.Message);
            }
            catch (InvalidOperationException iopex)
            {
                HandleDisconnect("Invalid operation: " + iopex.Message);
            }
            catch (UnauthorizedAccessException ua)
            {
                HandleDisconnect("Access denied: " + ua.Message);
            }
            catch (Exception ex)
            {
                labelStatus.Text = "Error: " + ex.Message;
            }
        }

        private void HandleDisconnect(string message)
        {
            try
            {
                if (serialPortArduino.IsOpen)
                {
                    try { serialPortArduino.Close(); } catch { }
                }
            }
            finally
            {
                timerOefening5.Stop();
                radioButtonVerbonden.Checked = false;
                buttonConnect.Text = "Connect";
                labelStatus.Text = "Disconnected: " + message;
                MessageBox.Show("Connection lost: " + message + "\nTimer stopped.", "Disconnected", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private int ParseAnalogReply(string reply)
        {
            if (string.IsNullOrEmpty(reply)) return 0;
            var parts = reply.Split(':');
            if (parts.Length >= 2)
            {
                var numPart = parts[1].Trim();
                int val = 0;
                int i = 0;
                while (i < numPart.Length && !char.IsDigit(numPart[i])) i++;
                int j = i;
                while (j < numPart.Length && char.IsDigit(numPart[j])) j++;
                if (i < j)
                {
                    int.TryParse(numPart.Substring(i, j - i), out val);
                    return val;
                }
            }
            var digits = new string(reply.Where(c => char.IsDigit(c)).ToArray());
            if (digits.Length > 0) { if (int.TryParse(digits, out int v2)) return v2; }
            return 0;
        }
    }
}

