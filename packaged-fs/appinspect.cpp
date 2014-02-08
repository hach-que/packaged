/* vim: set ts=4 sw=4 tw=0 et ai :*/

#include <libpackaged-fs/logging.h>
#include <libpackaged-fs/fsfile.h>
#include <libpackaged-fs/lowlevel/fs.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#ifdef WIN32
#include <conio.h>
#else
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int _getch()
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

namespace Program
{
    typedef void (*CommandFunc)(std::vector<std::string>);
    std::map<std::string, CommandFunc> AvailableCommands;
    AppLib::LowLevel::BlockStream * FSStream;
    AppLib::LowLevel::FS * FS;
    std::string TypeNames[256];
    char TypeChars[256];
}

void DoHelp(std::vector<std::string>);
void GetChildren(std::vector<std::string>);
void DoSegments(std::vector<std::string>);
void DoClean(std::vector<std::string>);
void DoShow(std::vector<std::string>);
std::pair<std::vector<uint32_t>, std::vector<uint32_t> > GetDataBlocks(uint32_t pos);
std::string ReadLine();
std::vector<std::string> ParseCommand(std::string cmd);
bool CheckArguments(std::string name, std::vector<std::string> args, int argcount);
bool CheckArguments(std::string name, std::vector<std::string> args, int minargs, int maxargs);
void SetTypeNames();

int main(int argc, char *argv[])
{
    AppLib::Logging::setApplicationName("appinspect");
    AppLib::Logging::verbose = true;

    // Check arguments.
    if (argc != 2)
    {
        AppLib::Logging::showErrorW("Invalid arguments provided.");
        AppLib::Logging::showErrorO("Usage: appinspect <filename>");
        return 1;
    }

    // Set up the array of command names and their functions.
    Program::AvailableCommands["help"] = &DoHelp;
    Program::AvailableCommands["children"] = &GetChildren;
    Program::AvailableCommands["segments"] = &DoSegments;
    Program::AvailableCommands["clean"] = &DoClean;
    Program::AvailableCommands["show"] = &DoShow;

    // Set the type names up.
    SetTypeNames();

    // Open the data stream.
    Program::FSStream = new AppLib::LowLevel::BlockStream(argv[1]);
    if (!Program::FSStream->is_open())
    {
        AppLib::Logging::showErrorW("Unable to open specified file as a block stream.  Make");
        AppLib::Logging::showErrorO("sure the file is not currently in use.");
        return 1;
    }

    // Open the package.
    Program::FS = new AppLib::LowLevel::FS(Program::FSStream);

    // Package is now open.  Show the initial filesystem information and
    // start the main application loop.
    AppLib::LowLevel::INode node = Program::FS->getINodeByPosition(OFFSET_FSINFO);
    AppLib::Logging::showInfoW("INode ID: %i", node.inodeid);
    AppLib::Logging::showInfoO("INode Type: %i", node.type);
    AppLib::Logging::showInfoO("Filesystem Name: %s", node.fs_name);
    AppLib::Logging::showInfoO("Filesystem Version: %i.%i.%i", node.ver_major, node.ver_minor, node.ver_revision);
    AppLib::Logging::showInfoO("Application Name: %s", node.app_name);
    AppLib::Logging::showInfoO("Application Version: %s", node.app_ver);
    AppLib::Logging::showInfoO("Application Description: %s", node.app_desc);
    AppLib::Logging::showInfoO("Application Author: %s", node.app_author);
    AppLib::Logging::showInfoO("Position of root directory INode: %p", node.pos_root);
    AppLib::Logging::showInfoO("Position of freelist INode: %p", node.pos_freelist);
    
    while (true)
    {
        std::cout << ">> ";
        std::string cmdstr = ReadLine();
        std::vector<std::string> command = ParseCommand(cmdstr);
        while (command.size() != 0 && command[0] == "__continue")
        {
            std::cout << ".. ";
            cmdstr += ReadLine();
            command = ParseCommand(cmdstr);
        }
        if (command.size() == 0)
        {
            std::cout << "# Bad input." << std::endl;
            continue;
        }
        
        if (command[0] == "exit")
            return 0;

        for (std::map<std::string, Program::CommandFunc>::iterator iter = Program::AvailableCommands.begin(); iter != Program::AvailableCommands.end(); iter++)
        {
            if (command[0] == iter->first)
            {
                iter->second(command);
                goto aftercmd_standard;
            }
        }
        
        std::cout << "# Bad command." << std::endl;
aftercmd_standard:
        continue;
    }
}

/// <summary>
/// Show a help page listing all of the available commands.
/// </summary>
void DoHelp(std::vector<std::string> cmd)
{
    printf("children <inode id> - List the children of the specified INode by ID.  Use 0 for the root INode.\n");
    printf("show <block num>    - Shows the binary representation of a block.\n");
    printf("segments            - Displays a representation of the types of each block in the package.\n");
    printf("clean               - Removes any temporary or invalid blocks in the package.\n");
}

/// <summary>
/// Show the INodes and filenames of children of the specified INode.
/// </summary>
void GetChildren(std::vector<std::string> cmd)
{
    if (!CheckArguments("children", cmd, 1)) return;

    // Get the ID.
    std::istringstream ss(cmd[1]);
    int id;
    ss >> id;

    // Loop through all of the children.
    std::vector<AppLib::LowLevel::INode> children = Program::FS->getChildrenOfDirectory(id);
    printf("Children of directory with INode %i:\n", id);
    for (int i = 0; i < children.size(); i += 1)
    {
        printf(" * %i (%s)\n", children[i].inodeid, Program::TypeNames[children[i].type].c_str());
    }
}

/// <summary>
/// Show the structure of the disk image by showing the type of data in each 4096b segment.
/// </summary>
void DoSegments(std::vector<std::string> cmd)
{
    if (!CheckArguments("segments", cmd, 0)) return;

    std::pair<std::vector<uint32_t>, std::vector<uint32_t> > p = GetDataBlocks(Program::FS->getINodeByPosition(OFFSET_FSINFO).pos_root);
    std::vector<uint32_t> datablocks = p.first;
    std::vector<uint32_t> headerblocks = p.second;

    printf("_ = free block          F = file info       S = segment info\n");
    printf("# = data                D = directory       L = symbolic link\n");
    printf("T = temporary data      %% = freelist        H = hard link\n");
    printf("I = filesystem info     ? = invalid           = unset\n");
    printf("! = inaccessible (will be removed by the clean operation)\n");
    printf("\n");
    printf("Header blocks: %i\n", headerblocks.size());
    printf("Data blocks: %i\n", datablocks.size());
    printf("\n");
    printf("/===============================================================\\\n");
    printf("|");
    uint32_t pos = OFFSET_FSINFO;
    int i = 0;
    while (true)
    {
        try
        {
            AppLib::LowLevel::INode node = Program::FS->getINodeByPosition(pos);

            if (i == 16)
            {
                printf("\n");
                printf("+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+\n");
                printf("|");
                i = 1;
            }
            else
                i += 1;

            if (Program::FS->isBlockFree(pos))
                printf(" _ |");
            else
            {
                bool data = false;
                bool header = false;
                for (int a = 0; a < datablocks.size(); a += 1)
                    if (datablocks[a] == pos)
                    {
                        data = true;
                        break;
                    }
                for (int a = 0; a < headerblocks.size(); a += 1)
                    if (headerblocks[a] == pos)
                    {
                        header = true;
                        break;
                    }

                if (data)
                    printf(" # |");
                else if (node.type < 0 || node.type > 255 || Program::TypeChars[node.type] == 0)
                    printf(" ? |");
                else if (header || (node.type != AppLib::LowLevel::INodeType::INT_FILEINFO && node.type != AppLib::LowLevel::INodeType::INT_DIRECTORY))
                    printf(" %c |", Program::TypeChars[node.type]);
                else
                    printf(" %c!|", Program::TypeChars[node.type]);
            }

            pos += BSIZE_FILE;
        }
        catch (std::ifstream::failure e)
        {
            // End-of-file.
            Program::FSStream->clear();
            if (i == 16) printf("\n");
            for (int a = i + 1; a <= 16; a += 1)
            {
                if (a == 16)
                    printf("   |\n");
                else
                    printf("    ");
            }
            printf("\\===============================================================/\n");
            return;
        }
    }
}

/// <summary>
/// Cleans out the filesystem, removing any unused or unneeded blocks.
/// </summary>
void DoClean(std::vector<std::string> cmd)
{
    if (!CheckArguments("clean", cmd, 0)) return;

    std::pair<std::vector<uint32_t>, std::vector<uint32_t> > p = GetDataBlocks(Program::FS->getINodeByPosition(OFFSET_FSINFO).pos_root);
    std::vector<uint32_t> datablocks = p.first;
    std::vector<uint32_t> headerblocks = p.second;

    uint32_t pos = OFFSET_FSINFO;
    int failed = 0;
    int cleaned = 0;
    int cleaned_temporary = 0;
    int cleaned_invalid = 0;
    int cleaned_files = 0;
    int cleaned_directories = 0;
    int i = 0;
    while (true)
    {
        try
        {
            AppLib::LowLevel::INode node = Program::FS->getINodeByPosition(pos);

            if (!Program::FS->isBlockFree(pos))
            {
                bool data = false;
                bool header = false;
                for (int a = 0; a < datablocks.size(); a += 1)
                    if (datablocks[a] == pos)
                    {
                        data = true;
                        break;
                    }
                for (int a = 0; a < headerblocks.size(); a += 1)
                    if (headerblocks[a] == pos)
                    {
                        header = true;
                        break;
                    }

                if (!data && !header)
                {
                    // Check to see if we should 'free' it in the filesystem.
                    switch (node.type)
                    {
                    case AppLib::LowLevel::INodeType::INT_TEMPORARY:
                        if (Program::FS->resetBlock(pos) == AppLib::LowLevel::FSResult::E_SUCCESS)
                        {
                            cleaned += 1;
                            cleaned_temporary += 1;
                        }
                        else
                            failed += 1;
                        break;
                    case AppLib::LowLevel::INodeType::INT_INVALID:
                        if (Program::FS->resetBlock(pos) == AppLib::LowLevel::FSResult::E_SUCCESS)
                        {
                            cleaned += 1;
                            cleaned_invalid += 1;;
                        }
                        else
                            failed += 1;
                        break;
                    case AppLib::LowLevel::INodeType::INT_FILEINFO:
                        if (Program::FS->resetBlock(pos) == AppLib::LowLevel::FSResult::E_SUCCESS)
                        {
                            cleaned += 1;
                            cleaned_files += 1;;
                        }
                        else
                            failed += 1;
                        break;
                    case AppLib::LowLevel::INodeType::INT_DIRECTORY:
                        if (Program::FS->resetBlock(pos) == AppLib::LowLevel::FSResult::E_SUCCESS)
                        {
                            cleaned += 1;
                            cleaned_directories += 1;;
                        }
                        else
                            failed += 1;
                        break;
                    default:
                        // Do nothing?
                        break;
                    }
                }
            }

            pos += BSIZE_FILE;
        }
        catch (std::ifstream::failure e)
        {
            // End-of-file.
            Program::FSStream->clear();
            printf("Cleaned %i blocks (%i temporary, %i invalid, %i files, %i directories).\n",
                cleaned, cleaned_temporary, cleaned_invalid, cleaned_files, cleaned_directories);
            if (failed > 0)
                printf("%i blocks could not be freed during cleaning.\n", failed);
            return;
        }
    }
}

/// <summary>
/// Show the INodes and filenames of children of the specified INode.
/// </summary>
void DoShow(std::vector<std::string> cmd)
{
    if (!CheckArguments("show", cmd, 1)) return;

    // Get the ID.
    std::istringstream ss(cmd[1]);
    int id;
    ss >> id;

    // Calculate position.
    uint32_t pos = OFFSET_FSINFO + (id * BSIZE_FILE);

    // Show hexidecimal contents of block.
    std::stringstream charcache;
    int a = 0;
    int b = 0;
    for (int i = 0; i < BSIZE_FILE; i++)
    {
        char hd;
        Program::FSStream->seekg(pos + i);
        AppLib::LowLevel::Endian::doR(Program::FSStream, &hd, 1);
        printf("%02X ", (unsigned char)hd);
        if (hd < 32 || hd == 127)
            charcache << "  ";
        else
            charcache << (unsigned char)hd << " ";

        a += 1;
        if (a >= 16)
        {
            printf(" %s\n", charcache.str().c_str());
            charcache.str("");
            a = 0;
        }

        b += 1;
        if (b >= 16 * 16 && i != BSIZE_FILE - 1)
        {
            printf("Showing %04X to %04X (page %i of %i). Press any key to view next 256 bytes.",
                i + 1 - (16 * 16), i, ((i + 1 - (16 * 16)) / 256) + 1, 16);
            _getch();
            printf("\n");
            b = 0;
        }
    }
}

/// <summary>
/// Gets the data and header blocks that can be accessed from the directory located at the specified position.
/// </summary>
/// <returns>
/// The first pair value is a set of data blocks, the second is a set of header blocks.  Combined they address
/// all of the accessible file and directory headers and data.
/// </returns>
std::pair<std::vector<uint32_t>, std::vector<uint32_t> > GetDataBlocks(uint32_t pos)
{
    AppLib::LowLevel::INode node = Program::FS->getINodeByPosition(pos);
    std::vector<uint32_t> positions;

    // Loop through all of the children.
    std::vector<AppLib::LowLevel::INode> children = Program::FS->getChildrenOfDirectory(node.inodeid);
    std::pair<std::vector<uint32_t>, std::vector<uint32_t> > accessible;
    std::vector<uint32_t> headers;
    std::vector<uint32_t> result1;
    std::vector<uint32_t> result2;
    uint32_t spos;
    uint32_t bpos;
    headers.insert(headers.begin(), pos);

    for (uint16_t i = 0; i < children.size(); i += 1)
    {
        switch (children[i].type)
        {
        case AppLib::LowLevel::INodeType::INT_DIRECTORY:
            spos = Program::FS->getINodePositionByID(children[i].inodeid);
            accessible = GetDataBlocks(spos);
            std::merge(positions.begin(), positions.end(), accessible.first.begin(), accessible.first.end(), result1.begin());
            std::merge(headers.begin(), headers.end(), accessible.second.begin(), accessible.second.end(), result2.begin());
            positions = result1;
            headers = result2;
            break;
        case AppLib::LowLevel::INodeType::INT_FILEINFO:
            bpos = Program::FS->getINodePositionByID(children[i].inodeid);
            headers.insert(headers.begin(), bpos);
            int spos;
            for (int a = HSIZE_FILE; a < BSIZE_FILE; a += 4)
            {
                Program::FSStream->seekg(bpos + a);
                spos = 0;
                AppLib::LowLevel::Endian::doR(Program::FSStream, reinterpret_cast<char *>(&spos), 4);

                if (spos == 0)				
                    break;
                else
                {
                    positions.insert(positions.begin(), spos);
                }
            }
            break;
        default:
            // Do nothing.
            break;
        }
    }

    return std::pair<std::vector<uint32_t>, std::vector<uint32_t> >(positions, headers);
}

/// <summary>
/// Reads input from a pseudo command line.
/// </summary>
std::string ReadLine()
{
    char c = '\0';
    std::string ret = "";
    do
    {
        c = _getch();
        if (c == 0 || c == -32)
        {
            // Special key code was pressed.
            c = _getch();
            if (c == 72 || c == 80)
            {
            }
            continue;
        }
        if (c < 32 && c != '\b' && c != '\r' && c != '\n')
        {
            printf("(%i)", (int)c);
            continue;
        }
        switch (c)
        {
            case '\r':
            case '\n':
                std::cout << std::endl;
                break;
            case '\b':
                if (ret.size() > 0)
                {
                    ret = ret.substr(0, ret.length() - 1);
                    std::cout << "\b \b";
                }
                break;
            default:
                std::cout << c;
                ret += c;
                break;
        }
    }
    while (c != '\r' && c != '\n');
    return ret;
}

/// <summary>
/// Parses a given string as a command string.
/// </summary>
std::vector<std::string> ParseCommand(std::string cmd)
{
    std::vector<std::string> ret;
    std::string buf = "";
    bool quoteOn = false;
    bool isEscaping = false;
    for (int i = 0; i < cmd.length(); i += 1)
    {
        if (isEscaping)
        {
            buf += cmd[i];
            isEscaping = false;
        }
        else if (cmd[i] == '\\')
        {
            isEscaping = true;
            continue;
        }
        else if (cmd[i] == '"')
        {
            quoteOn = !quoteOn;
        }
        else if (cmd[i] == ' ' && !quoteOn && buf != "")
        {
            ret.insert(ret.end(), buf);
            buf = "";
        }
        else if (quoteOn || cmd[i] != ' ')
            buf += cmd[i];
    }
    if (buf.length() > 0)
    {
        ret.insert(ret.end(), buf);
        buf = "";
    }
    if (quoteOn)
    {
        // Continue line.
        std::vector<std::string> cont;
        cont.insert(cont.begin(), "__continue");
        return cont;
    }
    return ret;
}

/// <summary>
/// Ensures there is a valid number of arguments to the command.
/// </summary>
bool CheckArguments(std::string name, std::vector<std::string> args, int minargs, int maxargs)
{
    if (maxargs == -1) maxargs = minargs;
    // We subtract one because the first argument is the function call name.
    if (args.size() - 1 < minargs || args.size() - 1 > maxargs)
    {
        AppLib::Logging::showErrorW("Invalid number of arguments to command '%s'.", name.c_str());
        AppLib::Logging::showErrorO("Type 'help' for usage details.");
        return false;
    }
    return true;
}

bool CheckArguments(std::string name, std::vector<std::string> args, int argcount)
{
    return CheckArguments(name, args, argcount, argcount);
}

void SetTypeNames()
{
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_FREEBLOCK] = "free";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_FILEINFO] = "file info";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_SEGINFO] = "segment info";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_DATA] = "data";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_DIRECTORY] = "directory";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_SYMLINK] = "symbolic link";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_DEVICE] = "device";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_TEMPORARY] = "temporary data";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_FREELIST] = "freelist block";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_FSINFO] = "filesystem info";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_INVALID] = "invalid";
    Program::TypeNames[AppLib::LowLevel::INodeType::INT_UNSET] = "unset";

    Program::TypeChars[AppLib::LowLevel::INodeType::INT_FREEBLOCK] = '_';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_FILEINFO] = 'F';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_SEGINFO] = 'S';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_DATA] = '#';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_DIRECTORY] = 'D';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_SYMLINK] = 'L';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_DEVICE] = 'D';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_TEMPORARY] = 'T';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_FREELIST] = '%';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_FSINFO] = 'I';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_INVALID] = '?';
    Program::TypeChars[AppLib::LowLevel::INodeType::INT_UNSET] = ' ';
}
