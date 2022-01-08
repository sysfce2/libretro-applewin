//
//  DiskImageBrowserWindowController.mm
//  Mariani
//
//  Created by sh95014 on 1/8/22.
//

#import "DiskImageBrowserWindowController.h"
#import "AppDelegate.h"
#import "DiskImg.h"

using namespace DiskImgLib;

#define VERTICAL_MARGIN             2

@interface FSItem : NSObject
@property (strong) NSString *name;
@property (strong) NSString *lastModified;
@property (strong) NSString *size;
@property (strong) NSString *kind;
@property (strong) NSMutableArray *children;
@end

@implementation FSItem
@end

@interface DiskImageBrowserWindowController ()

@property (strong) IBOutlet NSOutlineView *filesOutlineView;

@property (strong) NSString *path;
@property (strong) FSItem *rootDirectory;
@property CGFloat rowHeight;
@property NSDateFormatter *dateFormatter;

@end

@implementation DiskImageBrowserWindowController

NSArray *fileTypeStrings = @[
    @"NON", @"BAD", @"PCD", @"PTX", @"TXT", @"PDA", @"BIN", @"FNT",
    @"FOT", @"BA3", @"DA3", @"WPF", @"SOS", @"$0D", @"$0E", @"DIR",
    @"RPD", @"RPI", @"AFD", @"AFM", @"AFR", @"SCL", @"PFS", @"$17",
    @"$18", @"ADB", @"AWP", @"ASP", @"$1C", @"$1D", @"$1E", @"$1F",
    @"TDM", @"$21", @"$22", @"$23", @"$24", @"$25", @"$26", @"$27",
    @"$28", @"$29", @"8SC", @"8OB", @"8IC", @"8LD", @"P8C", @"$2F",
    @"$30", @"$31", @"$32", @"$33", @"$34", @"$35", @"$36", @"$37",
    @"$38", @"$39", @"$3A", @"$3B", @"$3C", @"$3D", @"$3E", @"$3F",
    @"DIC", @"OCR", @"FTD", @"$43", @"$44", @"$45", @"$46", @"$47",
    @"$48", @"$49", @"$4A", @"$4B", @"$4C", @"$4D", @"$4E", @"$4F",
    @"GWP", @"GSS", @"GDB", @"DRW", @"GDP", @"HMD", @"EDU", @"STN",
    @"HLP", @"COM", @"CFG", @"ANM", @"MUM", @"ENT", @"DVU", @"FIN",
    @"$60", @"$61", @"$62", @"$63", @"$64", @"$65", @"$66", @"$67",
    @"$68", @"$69", @"$6A", @"BIO", @"$6C", @"TDR", @"PRE", @"HDV",
    @"$70", @"$71", @"$72", @"$73", @"$74", @"$75", @"$76", @"$77",
    @"$78", @"$79", @"$7A", @"$7B", @"$7C", @"$7D", @"$7E", @"$7F",
    @"$80", @"$81", @"$82", @"$83", @"$84", @"$85", @"$86", @"$87",
    @"$88", @"$89", @"$8A", @"$8B", @"$8C", @"$8D", @"$8E", @"$8F",
    @"$90", @"$91", @"$92", @"$93", @"$94", @"$95", @"$96", @"$97",
    @"$98", @"$99", @"$9A", @"$9B", @"$9C", @"$9D", @"$9E", @"$9F",
    @"WP ", @"$A1", @"$A2", @"$A3", @"$A4", @"$A5", @"$A6", @"$A7",
    @"$A8", @"$A9", @"$AA", @"GSB", @"TDF", @"BDF", @"$AE", @"$AF",
    @"SRC", @"OBJ", @"LIB", @"S16", @"RTL", @"EXE", @"PIF", @"TIF",
    @"NDA", @"CDA", @"TOL", @"DVR", @"LDF", @"FST", @"$BE", @"DOC",
    @"PNT", @"PIC", @"ANI", @"PAL", @"$C4", @"OOG", @"SCR", @"CDV",
    @"FON", @"FND", @"ICN", @"$CB", @"$CC", @"$CD", @"$CE", @"$CF",
    @"$D0", @"$D1", @"$D2", @"$D3", @"$D4", @"MUS", @"INS", @"MDI",
    @"SND", @"$D9", @"$DA", @"DBM", @"$DC", @"DDD", @"$DE", @"$DF",
    @"LBR", @"$E1", @"ATK", @"$E3", @"$E4", @"$E5", @"$E6", @"$E7",
    @"$E8", @"$E9", @"$EA", @"$EB", @"$EC", @"$ED", @"R16", @"PAS",
    @"CMD", @"$F1", @"$F2", @"$F3", @"$F4", @"$F5", @"$F6", @"$F7",
    @"$F8", @"OS ", @"INT", @"IVR", @"BAS", @"VAR", @"REL", @"SYS",
];

- (instancetype)initWithDiskImageWrapper:(DiskImageWrapper *)wrapper {
    if ((self = [super init]) != nil) {
        self.path = wrapper.path;
        
        self.dateFormatter = [[NSDateFormatter alloc] init];
        self.dateFormatter.dateStyle = NSDateFormatterMediumStyle;
        self.dateFormatter.timeStyle = NSDateFormatterShortStyle;

        [self readFileSystem:wrapper];
        
        if (![[NSBundle mainBundle] loadNibNamed:@"DiskImageBrowser" owner:self topLevelObjects:nil]) {
            NSLog(@"failed to load DiskImageBrowser nib");
            return nil;
        }
        if ([self.rootDirectory.name length]) {
            self.window.title = [NSString stringWithFormat:@"%@ (%@)", self.rootDirectory.name, [wrapper.path lastPathComponent]];
        }
        else {
            self.window.title = [wrapper.path lastPathComponent];
        }
    }
    return self;
}

#pragma mark - NSWindowDelegate

- (void)windowWillClose:(NSNotification *)notification {
    [theAppDelegate browserWindowWillClose:self.path];
}

#pragma mark - NSOutlineViewDataSource

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(nullable id)item {
    if (item == nil) {
        item = self.rootDirectory;
    }
    FSItem *fsItem = (FSItem *)item;
    return fsItem.children.count;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(nullable id)item {
    if (item == nil) {
        item = self.rootDirectory;
    }
    FSItem *fsItem = (FSItem *)item;
    return [fsItem.children objectAtIndex:index];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    if (item == nil) {
        item = self.rootDirectory;
    }
    FSItem *fsItem = (FSItem *)item;
    return (fsItem.children.count > 0);
}

#pragma mark - NSOutlineViewDelegate

- (CGFloat)outlineView:(NSOutlineView *)outlineView heightOfRowByItem:(id)item {
    if (self.rowHeight == 0) {
        self.rowHeight = [NSFont systemFontOfSize:[NSFont systemFontSize]].boundingRectForFont.size.height + VERTICAL_MARGIN * 2;
    }
    return self.rowHeight;
}

- (nullable NSView *)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(nullable NSTableColumn *)tableColumn item:(id)item {
    NSView *enclosingView = [[NSView alloc] initWithFrame:CGRectMake(0, 0, 0, self.rowHeight)];
    FSItem *fsItem = (FSItem *)item;
    
    NSTextField *textField = [[NSTextField alloc] init];
    if ([tableColumn.identifier isEqualToString:@"name"]) {
        textField.stringValue = fsItem.name;
    }
    else if ([tableColumn.identifier isEqualToString:@"dateModified"]) {
        textField.stringValue = fsItem.lastModified;
    }
    else if ([tableColumn.identifier isEqualToString:@"size"]) {
        textField.stringValue = fsItem.size;
    }
    else if ([tableColumn.identifier isEqualToString:@"kind"]) {
        textField.stringValue = fsItem.kind;
    }
    textField.bezeled = NO;
    textField.drawsBackground = NO;
    textField.editable = NO;
    textField.selectable = NO;
    [textField sizeToFit];
    
    // center textField vertically within enclosingView
    CGRect textFieldFrame = textField.frame;
    textFieldFrame.origin.y = VERTICAL_MARGIN;
    textField.frame = textFieldFrame;
    
    [enclosingView addSubview:textField];
    return enclosingView;
}

#pragma mark - Utilities

- (void)readFileSystem:(DiskImageWrapper *)wrapper {
    DiskFS *diskFS = wrapper.diskImg->OpenAppropriateDiskFS();
    diskFS->SetScanForSubVolumes(DiskFS::kScanSubEnabled);
    diskFS->Initialize(wrapper.diskImg, DiskFS::kInitFull);
    
    NSMutableDictionary *directories = [NSMutableDictionary dictionary];
    
    // in case there's no volume directory (which is the case with DOS 3.3
    // disks), let's still have a root directory to catch the files
    self.rootDirectory = [[FSItem alloc] init];
    self.rootDirectory.children = [NSMutableArray array];
    FSItem *parentFsItem = self.rootDirectory;
    
    A2File *file;
    file = diskFS->GetNextFile(nil);
    while (file != nil) {
        FSItem *fsItem = [self newFSItemFromA2File:file];
        if (file->IsDirectory()) {
            fsItem.children = [NSMutableArray array];
            [directories setObject:fsItem forKey:[NSString stringWithUTF8String:file->GetPathName()]];
            if (file->IsVolumeDirectory()) {
                self.rootDirectory = fsItem;
            }
        }
        A2File *parent = file->GetParent();
        if (parent != NULL) {
            // add myself to my parent's list of children
            parentFsItem = [directories objectForKey:[NSString stringWithUTF8String:parent->GetPathName()]];
        }
        [parentFsItem.children addObject:fsItem];
        
        file = diskFS->GetNextFile(file);
    }
    delete diskFS;
}

- (FSItem *)newFSItemFromA2File:(A2File *)file {
    static dispatch_once_t onceToken;
    static NSString *blankFilename = nil;
    dispatch_once(&onceToken, ^{
        blankFilename = NSLocalizedString(@"Unnamed", @"");
    });
    
    FSItem *fsItem = [[FSItem alloc] init];
    
    fsItem.name = [NSString stringWithUTF8String:file->GetFileName()];
    if ([fsItem.name length] == 0) {
        fsItem.name = blankFilename;
    }
    
    time_t modWhen = file->GetModWhen();
    if (modWhen != 0) {
        NSDate *date = [NSDate dateWithTimeIntervalSince1970:file->GetModWhen()];
        fsItem.lastModified = [self.dateFormatter stringFromDate:date];
    }
    else {
        fsItem.lastModified = NSLocalizedString(@"-", @"no date available");
    }
    
    uint64_t size;
    if (file->IsDirectory()) {
        size = file->GetDataLength();
    } else if (file->GetRsrcLength() >= 0) {
        size = file->GetDataLength() + file->GetRsrcLength();
    } else {
        size = file->GetDataLength();
    }
    if (size > 999999999) {
        fsItem.size = [NSString stringWithFormat:NSLocalizedString(@"%.1f GB", @""), (double)size / 1000000000];
    }
    else if (size > 999999) {
        fsItem.size = [NSString stringWithFormat:NSLocalizedString(@"%.1f MB", @""), (double)size / 1000000];
    }
    else if (size > 999) {
        fsItem.size = [NSString stringWithFormat:NSLocalizedString(@"%.1f KB", @""), (double)size / 1000];
    }
    else if (size == 2) {
        fsItem.size = NSLocalizedString(@"2 bytes", @"");
    }
    else if (size == 1) {
        fsItem.size = NSLocalizedString(@"1 byte", @"");
    }
    else if (size == 0) {
        fsItem.size = NSLocalizedString(@"0 bytes", @"");
    }
    else {
        fsItem.size = [NSString stringWithFormat:NSLocalizedString(@"%llu bytes", @""), size];
    }
    
    fsItem.kind = [NSString stringWithFormat:@"%@ $%04X", fileTypeStrings[file->GetFileType()], file->GetAuxType()];
    
    return fsItem;
}

@end
