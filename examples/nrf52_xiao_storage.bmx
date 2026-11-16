SuperStrict

Framework BRL.StandardIO
Import BRL.FileSystem
Import BRL.TextStream
Import NRF52.Storage.LittleFS

Delay 2000
Local result:String

If Not LittleFSIsMounted() Then
    result = "STORAGE,mount=fail,error=" + LittleFSLastError()
Else
    Local boots:Int
    If FileType("littlefs::boots.txt") = FILETYPE_FILE Then
        boots = Int(LoadText("littlefs::boots.txt"))
    End If
    boots :+ 1
    SaveText(String(boots), "boots.txt")
    If LoadText("file::boots.txt") = String(boots) Then
        Local operations:Int = True
        If FileType("storage-test") = FILETYPE_NONE Then
            operations = CreateDir("storage-test")
        End If
        If operations Then operations = SaveText("roundtrip", "storage-test/original.txt")
        If operations Then operations = RenameFile("storage-test/original.txt", "storage-test/moved.txt")
        If operations Then operations = LoadText("littlefs::storage-test/moved.txt") = "roundtrip"
        Local directory:Byte Ptr
        If operations Then directory = ReadDir("storage-test")
        operations = operations And directory <> Null
        Local found:Int
        If directory Then
            Local name:String = NextFile(directory)
            While name
                If name = "moved.txt" Then found = True
                name = NextFile(directory)
            Wend
            CloseDir(directory)
        End If
        operations = operations And found
        If operations Then operations = DeleteFile("storage-test/moved.txt")
        If operations Then operations = DeleteDir("storage-test")
        result = "STORAGE,mount=pass,read=pass,write=pass,ops=" + operations + ..
            ",boots=" + boots + ",used=" + LittleFSUsed() + ..
            ",capacity=" + LittleFSCapacity()
    Else
        result = "STORAGE,verify=fail,error=" + LittleFSLastError()
    End If
End If

While True
    Print result
    Delay 3000
Wend
