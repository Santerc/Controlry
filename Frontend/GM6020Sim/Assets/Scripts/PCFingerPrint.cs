using System;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

public static class PCFingerprint
{
    public static string GetFingerprint()
    {
        string raw = $"{Environment.MachineName}-{Environment.UserName}-{Environment.OSVersion}-{Environment.ProcessorCount}-{Environment.SystemDirectory}";
        return Sha256(raw);
    }

    private static string Sha256(string input)
    {
        using (var sha = SHA256.Create())
        {
            var bytes = Encoding.UTF8.GetBytes(input);
            var hash = sha.ComputeHash(bytes);
            return BitConverter.ToString(hash).Replace("-", "");
        }
    }
    
    public static int GetDeviceId()
    {
        string raw = $"{Environment.MachineName}-{Environment.UserName}-{Environment.OSVersion}-{Environment.ProcessorCount}-{Environment.SystemDirectory}";
        string hash = Sha256(raw);
        string prefix = hash.Substring(0, 8);
        int val = Convert.ToInt32(prefix, 16);
        int id = val % 10;
        return id;
    }
    
}