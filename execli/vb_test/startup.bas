Attribute VB_Name = "startup"
Option Explicit


Declare Function exec_cli Lib "libexecli" _
    (ByVal exe_name As String, _
     ByVal command_line As String, _
     ByVal current_dir As String, _
     ByVal callback As Long) As Long


Dim all As String


' a workaround to convert C-style ANSI string to VB-string
Type c_string
    text(1024) As Byte
End Type
Function callback(ByRef output As c_string, _
                  ByVal length As Long) As Long
    Dim i As Long, old_len As Long
    old_len = Len(all)
    all = all & Space$(length)
    For i = 0 To length - 1
        Mid$(all, old_len + i + 1, 1) = Chr$(output.text(i))
    Next i
End Function


Sub Main()

    Dim exit_code As Long
    all = ""
    exit_code = exec_cli("c:\windows\system32\cmd.exe", _
                         "/c dir", _
                         "c:\", _
                         AddressOf callback)
    MsgBox ("exit code: " & exit_code & vbCrLf & all)

End Sub
